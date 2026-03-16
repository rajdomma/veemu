package api

import (
	"encoding/json"
	"io"
	"net/http"
	"github.com/veemu/veemu-api/session"
)

func FlashHandler(mgr *session.Manager, baseURL string, onSession func(id,board string)) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		r.ParseMultipartForm(32 << 20)
		boardID := r.FormValue("board")
		if boardID == "" { boardID = "stm32f401re" }

		f, _, err := r.FormFile("elf")
		if err != nil { jsonError(w, "missing elf field", 400); return }
		defer f.Close()

		elf, err := io.ReadAll(f)
		if err != nil { jsonError(w, "read error", 500); return }
		if len(elf) < 4 || string(elf[:4]) != "\x7fELF" { jsonError(w, "not a valid ELF", 400); return }

		sess, err := mgr.Create(boardID, elf)
		if err != nil { jsonError(w, err.Error(), 500); return }

		if onSession != nil { onSession(sess.ID, sess.BoardID) }
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(201)
		json.NewEncoder(w).Encode(map[string]string{
			"session_id": sess.ID,
			"board_id":   sess.BoardID,
			"status":     string(sess.Status),
			"ws_live":    baseURL + "/live?session=" + sess.ID,
			"uart":       baseURL + "/uart?session=" + sess.ID,
		})
	}
}
