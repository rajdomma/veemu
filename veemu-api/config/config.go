package config

import "os"

type Config struct {
	ListenAddr     string
	BaseURL        string
	BoardConfigDir string
}

func Load() Config {
	return Config{
		ListenAddr:     envOr("VEEMU_LISTEN", ":8080"),
		BaseURL:        envOr("VEEMU_BASE_URL", "http://localhost:8080"),
		BoardConfigDir: envOr("VEEMU_BOARD_DIR", "../veemu-engine/boards"),
	}
}

func envOr(key, fallback string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	return fallback
}
