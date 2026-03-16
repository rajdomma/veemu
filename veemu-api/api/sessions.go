package api

import (
	"encoding/json"
	"net/http"
	"strings"
	"github.com/veemu/veemu-api/session"
)

func SessionsHandler(mgr *session.Manager) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		list := mgr.List()
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]any{"sessions": list, "count": len(list)})
	}
}

func SessionDeleteHandler(mgr *session.Manager) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		// extract id from /sessions/{id}
		id := strings.TrimPrefix(r.URL.Path, "/sessions/")
		if err := mgr.Destroy(id); err != nil { jsonError(w, "not found", 404); return }
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]string{"status": "destroyed", "id": id})
	}
}
