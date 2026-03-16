// file_server.go — add to veemu-api/
// New endpoints:
//   GET  /file?app=<app>&path=<path>   read a file from veemu-app/
//   POST /file   {app, path, content}   write a file to veemu-app/
//   POST /build  {app}                  run make APP=<app>, return {success,output,elf_path}
//   POST /flash-app {app,elf_path,board} flash from built ELF on disk
//   GET  /latest-session               return latest known session_id

package main

import (
	"github.com/gorilla/mux"
	"github.com/veemu/veemu-api/session"
	"bytes"
	"encoding/json"
	"fmt"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"sync"
	"time"
)

// veemuAppDir is resolved relative to the binary or overridden by env var.
// Set VEEMU_APP_DIR in your env (the root Makefile already does this for `make start`).
func veemuAppDir() string {
	if d := os.Getenv("VEEMU_APP_DIR"); d != "" {
		return d
	}
	// fallback: sibling directory
	exe, _ := os.Executable()
	return filepath.Join(filepath.Dir(exe), "..", "veemu-app")
}

// ── latest-session tracking ──────────────────────────────────────────────────

var (
	latestSessionMu sync.RWMutex
	latestSessionID string
	latestBoardID   string
)

func recordLatestSession(sessionID, boardID string) {
	latestSessionMu.Lock()
	defer latestSessionMu.Unlock()
	latestSessionID = sessionID
	latestBoardID = boardID
}

// ── handlers ─────────────────────────────────────────────────────────────────

func handleGetFile(w http.ResponseWriter, r *http.Request) {
	app := r.URL.Query().Get("app")
	path := r.URL.Query().Get("path")
	if app == "" || path == "" {
		http.Error(w, "missing app or path", http.StatusBadRequest)
		return
	}
	// Sanitize: no ".." traversal
	clean := filepath.Clean(path)
	if strings.HasPrefix(clean, "..") {
		http.Error(w, "invalid path", http.StatusBadRequest)
		return
	}
	full := filepath.Join(veemuAppDir(), clean)
	data, err := os.ReadFile(full)
	if err != nil {
		http.Error(w, fmt.Sprintf("file not found: %s", err), http.StatusNotFound)
		return
	}
	w.Header().Set("Content-Type", "text/plain; charset=utf-8")
	w.Header().Set("Access-Control-Allow-Origin", "*")
	w.Write(data)
}

func handlePostFile(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Access-Control-Allow-Origin", "*")
	if r.Method == http.MethodOptions {
		w.Header().Set("Access-Control-Allow-Methods", "POST, OPTIONS")
		w.Header().Set("Access-Control-Allow-Headers", "Content-Type")
		w.WriteHeader(http.StatusNoContent)
		return
	}
	var req struct {
		App     string `json:"app"`
		Path    string `json:"path"`
		Content string `json:"content"`
	}
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		http.Error(w, "bad JSON: "+err.Error(), http.StatusBadRequest)
		return
	}
	clean := filepath.Clean(req.Path)
	if strings.HasPrefix(clean, "..") {
		http.Error(w, "invalid path", http.StatusBadRequest)
		return
	}
	full := filepath.Join(veemuAppDir(), clean)
	if err := os.MkdirAll(filepath.Dir(full), 0755); err != nil {
		http.Error(w, "mkdir failed: "+err.Error(), http.StatusInternalServerError)
		return
	}
	if err := os.WriteFile(full, []byte(req.Content), 0644); err != nil {
		http.Error(w, "write failed: "+err.Error(), http.StatusInternalServerError)
		return
	}
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(map[string]any{"ok": true, "path": clean})
}

func handleBuild(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Access-Control-Allow-Origin", "*")
	if r.Method == http.MethodOptions {
		w.Header().Set("Access-Control-Allow-Methods", "POST, OPTIONS")
		w.Header().Set("Access-Control-Allow-Headers", "Content-Type")
		w.WriteHeader(http.StatusNoContent)
		return
	}
	var req struct {
		App string `json:"app"`
	}
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil || req.App == "" {
		http.Error(w, "missing app", http.StatusBadRequest)
		return
	}
	// Sanitize app name
	for _, c := range req.App {
		if !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
			http.Error(w, "invalid app name", http.StatusBadRequest)
			return
		}
	}
	appDir := veemuAppDir()
	cmd := exec.Command("make", "APP="+req.App)
	cmd.Dir = appDir
	var out bytes.Buffer
	cmd.Stdout = &out
	cmd.Stderr = &out
	err := cmd.Run()
	success := err == nil
	elfPath := fmt.Sprintf("build/%s.elf", req.App)
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(map[string]any{
		"success":  success,
		"output":   out.String(),
		"elf_path": elfPath,
		"app":      req.App,
	})
}

// handleFlashApp flashes an ELF that already exists on disk (no upload needed).
// Reuses the existing flashELF logic — reads the file and delegates.
func handleFlashApp(mgr *session.Manager) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Access-Control-Allow-Origin", "*")
	if r.Method == http.MethodOptions {
		w.Header().Set("Access-Control-Allow-Methods", "POST, OPTIONS")
		w.Header().Set("Access-Control-Allow-Headers", "Content-Type")
		w.WriteHeader(http.StatusNoContent)
		return
	}
	var req struct {
		App     string `json:"app"`
		ElfPath string `json:"elf_path"`
		Board   string `json:"board"`
	}
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		http.Error(w, "bad JSON: "+err.Error(), http.StatusBadRequest)
		return
	}
	if req.App == "" {
		http.Error(w, "missing app", http.StatusBadRequest)
		return
	}
	if req.ElfPath == "" {
		req.ElfPath = fmt.Sprintf("build/%s.elf", req.App)
	}
	if req.Board == "" {
		req.Board = "stm32f401re"
	}
	elfFull := filepath.Join(veemuAppDir(), filepath.Clean(req.ElfPath))
	elfData, err := os.ReadFile(elfFull)
	if err != nil {
		http.Error(w, fmt.Sprintf("ELF not found at %s — run Build first: %v", elfFull, err), http.StatusNotFound)
		return
	}

	// Delegate to the existing flash logic by creating a fake multipart-like call
	// We reuse the board engine directly here.
	boardDir := os.Getenv("VEEMU_BOARD_DIR")
	if boardDir == "" {
		boardDir = filepath.Join(filepath.Dir(veemuAppDir()), "veemu-engine", "boards")
	}
	session, err := mgr.Create(req.Board, elfData)
	if err != nil {
		http.Error(w, "flash failed: "+err.Error(), http.StatusInternalServerError)
		return
	}
	recordLatestSession(session.ID, req.Board)
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(map[string]any{
		"session_id": session.ID,
		"board_id":   req.Board,
		"status":     "running",
		"app":        req.App,
	})
	}
}

func handleLatestSession(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Access-Control-Allow-Origin", "*")
	w.Header().Set("Content-Type", "application/json")
	latestSessionMu.RLock()
	sid := latestSessionID
	bid := latestBoardID
	latestSessionMu.RUnlock()
	json.NewEncoder(w).Encode(map[string]any{
		"session_id": sid,
		"board_id":   bid,
		"ts":         time.Now().Unix(),
	})
}

// RegisterFileServerRoutes wires all new endpoints into an existing ServeMux.
// Call this from main.go after setting up existing routes.
func RegisterFileServerRoutes(mux *mux.Router, mgr *session.Manager) {
	mux.HandleFunc("/file", func(w http.ResponseWriter, r *http.Request) {
		switch r.Method {
		case http.MethodGet:
			handleGetFile(w, r)
		case http.MethodPost:
			handlePostFile(w, r)
		case http.MethodOptions:
			w.Header().Set("Access-Control-Allow-Origin", "*")
			w.Header().Set("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
			w.Header().Set("Access-Control-Allow-Headers", "Content-Type")
			w.WriteHeader(http.StatusNoContent)
		default:
			http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
		}
	})
	mux.HandleFunc("/build", handleBuild)
	mux.HandleFunc("/flash-app", handleFlashApp(mgr))
	mux.HandleFunc("/latest-session", handleLatestSession)
}
