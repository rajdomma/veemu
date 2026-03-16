package session

import (
	"errors"
	"log"
	"sync"
	"time"

	"github.com/google/uuid"
	"github.com/veemu/veemu-api/engine"
)

type Status string

const (
	StatusBooting Status = "booting"
	StatusRunning Status = "running"
	StatusStopped Status = "stopped"
	StatusError   Status = "error"
)

type Event struct {
	Type      string    `json:"type"`
	Timestamp time.Time `json:"ts"`
	Data      string    `json:"data,omitempty"`
	Port      string    `json:"port,omitempty"`
	Pin       uint8     `json:"pin,omitempty"`
	State     bool      `json:"state,omitempty"`
}

type Session struct {
	ID        string
	BoardID   string
	Status    Status
	CreatedAt time.Time
	instance  *engine.Instance
	uartBuf   []byte
	uartMu    sync.Mutex
	events    chan Event
	stopCh    chan struct{}
	doneCh    chan struct{}
	readyCh   chan struct{}
}

type Manager struct {
	mu       sync.RWMutex
	sessions map[string]*Session
	boardDir string
}

func NewManager(boardDir string) *Manager {
	return &Manager{sessions: make(map[string]*Session), boardDir: boardDir}
}

func (m *Manager) Create(boardID string, elf []byte) (*Session, error) {
	inst, err := engine.NewInstance(m.boardDir + "/" + boardID + ".json")
	if err != nil {
		return nil, err
	}
	if err := inst.LoadELF(elf); err != nil {
		inst.Destroy()
		return nil, err
	}
	if err := inst.Reset(); err != nil {
		inst.Destroy()
		return nil, err
	}
	s := &Session{
		ID:        uuid.NewString(),
		BoardID:   boardID,
		Status:    StatusBooting,
		CreatedAt: time.Now(),
		instance:  inst,
		events:    make(chan Event, 512),
		stopCh:    make(chan struct{}),
		doneCh:    make(chan struct{}),
		readyCh:   make(chan struct{}),
	}
	m.mu.Lock()
	m.sessions[s.ID] = s
	m.mu.Unlock()
	go s.runLoop()
	close(s.readyCh) // unblock runLoop after reset is done
	return s, nil
}

func (m *Manager) Get(id string) (*Session, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	s, ok := m.sessions[id]
	if !ok {
		return nil, errors.New("session not found")
	}
	return s, nil
}

func (m *Manager) Destroy(id string) error {
	m.mu.Lock()
	s, ok := m.sessions[id]
	if ok {
		delete(m.sessions, id)
	}
	m.mu.Unlock()
	if !ok {
		return errors.New("session not found")
	}
	s.stop()
	return nil
}

func (m *Manager) List() []SessionInfo {
	m.mu.RLock()
	defer m.mu.RUnlock()
	out := make([]SessionInfo, 0, len(m.sessions))
	for _, s := range m.sessions {
		out = append(out, s.Info())
	}
	return out
}

func (s *Session) runLoop() {
	defer close(s.doneCh)
	defer s.instance.Destroy()
	s.Status = StatusRunning

	// STM32F401RE runs at 16MHz HSI.
	// SysTick LOAD = 15999 → fires every 16000 instructions = 1ms.
	// To emulate at real wall-clock speed:
	//   run exactly 16000 instructions every 1ms tick.
	// This means delay_ms(150) in firmware takes a real 150ms.
	//
	// We batch into 1ms slices: 1ms ticker, 16_000 insns per slice.
	const (
		cpuHz         = 16_000_000      // 16 MHz HSI
		sliceHz       = 1_000           // 1ms slices
		insnsPerSlice = cpuHz / sliceHz // 16_000
	)

	tick := time.NewTicker(time.Millisecond)
	defer tick.Stop()

	// Wait until reset is complete before running
	<-s.readyCh

	for {
		select {
		case <-s.stopCh:
			log.Printf("[session %s] stopped", s.ID)
			s.Status = StatusStopped
			return

		case <-tick.C:
			if err := s.instance.Run(insnsPerSlice); err != nil {
				log.Printf("[session %s] run error: %v", s.ID, err)
				s.Status = StatusError
				return
			}

			// Drain UART
			if data := s.instance.ReadUART(); len(data) > 0 {
				s.uartMu.Lock()
				s.uartBuf = append(s.uartBuf, data...)
				s.uartMu.Unlock()
				select {
				case s.events <- Event{
					Type:      "uart",
					Timestamp: time.Now(),
					Data:      string(data),
				}:
				default:
				}
			}

			// Drain GPIO events
			for _, g := range s.instance.GPIOEvents() {
				select {
				case s.events <- Event{
					Type:      "gpio",
					Timestamp: time.Now(),
					Port:      g.Port,
					Pin:       g.Pin,
					State:     g.State,
				}:
				default:
				}
			}
		}
	}
}

func (s *Session) stop()                { close(s.stopCh); <-s.doneCh }
func (s *Session) Events() <-chan Event { return s.events }
func (s *Session) InjectGPIO(port string, pin uint8, state bool) error {
	return s.instance.SetGPIOInput(port, pin, state)
}

func (s *Session) DrainUART() []byte {
	s.uartMu.Lock()
	defer s.uartMu.Unlock()
	out := make([]byte, len(s.uartBuf))
	copy(out, s.uartBuf)
	s.uartBuf = s.uartBuf[:0]
	return out
}

type SessionInfo struct {
	ID        string    `json:"id"`
	BoardID   string    `json:"board_id"`
	Status    Status    `json:"status"`
	CreatedAt time.Time `json:"created_at"`
}

func (s *Session) Info() SessionInfo {
	return SessionInfo{ID: s.ID, BoardID: s.BoardID, Status: s.Status, CreatedAt: s.CreatedAt}
}
