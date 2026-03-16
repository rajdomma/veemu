package api

import (
	"encoding/json"
	"net/http"

	"github.com/veemu/veemu-api/session"
)

type gpioInputRequest struct {
	Port  string `json:"port"`
	Pin   uint8  `json:"pin"`
	State bool   `json:"state"`
}

func GPIOInputHandler(mgr *session.Manager) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		sessionID := r.URL.Query().Get("session")
		if sessionID == "" {
			http.Error(w, `{"error":"missing session"}`, http.StatusBadRequest)
			return
		}

		var req gpioInputRequest
		if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
			http.Error(w, `{"error":"invalid body"}`, http.StatusBadRequest)
			return
		}

		s, err := mgr.Get(sessionID)
		if err != nil {
			http.Error(w, `{"error":"session not found"}`, http.StatusNotFound)
			return
		}

		if err := s.InjectGPIO(req.Port, req.Pin, req.State); err != nil {
			http.Error(w, `{"error":"`+err.Error()+`"}`, http.StatusInternalServerError)
			return
		}

		w.Header().Set("Content-Type", "application/json")
		w.Write([]byte(`{"ok":true}`))
	}
}
