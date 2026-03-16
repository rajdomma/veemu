package main

import (
	"log"
	"net/http"
	"os"
	"strings"
	"time"

	"github.com/gorilla/mux"
	"github.com/veemu/veemu-api/api"
	"github.com/veemu/veemu-api/config"
	"github.com/veemu/veemu-api/session"
)

func main() {
	cfg := config.Load()
	appDir := os.Getenv("VEEMU_APP_DIR")
	if appDir == "" { appDir = "../veemu-app" }
	_ = appDir

	mgr := session.NewManager(cfg.BoardConfigDir)

	r := mux.NewRouter()
	r.HandleFunc("/health", healthHandler).Methods("GET")
	r.HandleFunc("/flash", api.FlashHandler(mgr, cfg.BaseURL, api.RecordLatestSession)).Methods("POST")
	r.HandleFunc("/uart", api.UARTHandler(mgr)).Methods("GET")
	r.HandleFunc("/live", api.LiveHandler(mgr))
	r.HandleFunc("/sessions", api.SessionsHandler(mgr)).Methods("GET")
	r.PathPrefix("/sessions/").HandlerFunc(api.SessionDeleteHandler(mgr)).Methods("DELETE")
	r.HandleFunc("/gpio_input", api.GPIOInputHandler(mgr)).Methods("POST")
	r.HandleFunc("/latest-session", api.LatestSessionHandler(mgr)).Methods("GET")
	RegisterFileServerRoutes(r, mgr)

	srv := &http.Server{
		Addr:        cfg.ListenAddr,
		Handler:     cors(logger(r)),
		ReadTimeout: 30 * time.Second,
		IdleTimeout: 120 * time.Second,
	}
	log.Printf("[veemu-api] listening on %s", cfg.ListenAddr)
	log.Fatal(srv.ListenAndServe())
}

func healthHandler(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")
	w.Write([]byte(`{"status":"ok","service":"veemu-api"}`))
}

func logger(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		t := time.Now()
		next.ServeHTTP(w, r)
		log.Printf("%s %s %s", r.Method, r.URL.Path, time.Since(t))
	})
}

func cors(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Access-Control-Allow-Origin", "*")
		w.Header().Set("Access-Control-Allow-Methods", "GET,POST,DELETE,OPTIONS")
		w.Header().Set("Access-Control-Allow-Headers", "Content-Type")
		if strings.EqualFold(r.Method, "OPTIONS") {
			w.WriteHeader(204)
			return
		}
		next.ServeHTTP(w, r)
	})
}
