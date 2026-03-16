package api

import (
	"encoding/json"
	"net/http"
	"sync"
)

// latestSession tracks the most recently flashed session ID and board ID.
// Updated by RecordLatestSession (called from FlashHandler).
// Read by LatestSessionHandler (polled by frontend every 3-5s).

var (
	latestMu        sync.RWMutex
	latestSessionID string
	latestBoardID   string
)

// RecordLatestSession is called by FlashHandler after a successful flash.
func RecordLatestSession(id, board string) {
	latestMu.Lock()
	defer latestMu.Unlock()
	latestSessionID = id
	latestBoardID = board
}

// LatestSessionHandler returns the most recently flashed session.
// Frontend polls this to auto-sync terminal flashes.
func LatestSessionHandler(mgr interface{}) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		latestMu.RLock()
		id := latestSessionID
		board := latestBoardID
		latestMu.RUnlock()

		w.Header().Set("Content-Type", "application/json")
		if id == "" {
			w.WriteHeader(404)
			w.Write([]byte(`{"session_id":""}`))
			return
		}
		json.NewEncoder(w).Encode(map[string]string{
			"session_id": id,
			"board_id":   board,
		})
	}
}
