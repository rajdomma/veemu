package api

import (
	"encoding/json"
	"log"
	"net/http"
	"time"

	"github.com/gorilla/websocket"
	"github.com/veemu/veemu-api/session"
)

var upgrader = websocket.Upgrader{CheckOrigin: func(r *http.Request) bool { return true }}

func LiveHandler(mgr *session.Manager) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		id := r.URL.Query().Get("session")
		sess, err := mgr.Get(id)
		if err != nil { http.Error(w, "session not found", 404); return }

		conn, err := upgrader.Upgrade(w, r, nil)
		if err != nil { return }
		defer conn.Close()

		conn.WriteJSON(map[string]any{"type": "connected", "session_id": id, "board_id": sess.BoardID})

		ping := time.NewTicker(15 * time.Second)
		defer ping.Stop()

		for {
			select {
			case evt, ok := <-sess.Events():
				if !ok { conn.WriteJSON(map[string]string{"type": "session_ended"}); return }
				if err := conn.WriteJSON(evt); err != nil { log.Printf("[ws] write err: %v", err); return }
			case <-ping.C:
				conn.WriteMessage(websocket.PingMessage, nil)
			}
		}
	}
}

func jsonError(w http.ResponseWriter, msg string, code int) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(code)
	json.NewEncoder(w).Encode(map[string]string{"error": msg})
}
