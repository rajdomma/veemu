package api

import (
	"encoding/json"
	"net/http"
	"github.com/veemu/veemu-api/session"
)

func UARTHandler(mgr *session.Manager) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		id := r.URL.Query().Get("session")
		sess, err := mgr.Get(id)
		if err != nil { jsonError(w, "session not found", 404); return }

		data := sess.DrainUART()
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]any{
			"session_id": id,
			"output":     string(data),
			"bytes":      len(data),
		})
	}
}
