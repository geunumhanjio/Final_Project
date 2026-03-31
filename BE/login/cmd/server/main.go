package main

import (
	"log"
	"net/http"

	"github.com/gin-gonic/gin"

	"login/internal/api"
	"login/internal/auth"
	"login/internal/config"
	"login/internal/db"
	"login/internal/store"
	"login/internal/user"
)

func main() {
	cfg, err := config.Load()
	if err != nil {
		log.Fatalf("failed to load config: %v", err)
	}

	sqlDB, err := db.OpenAndMigrate(cfg)
	if err != nil {
		log.Fatalf("failed to initialize mysql: %v", err)
	}
	defer sqlDB.Close()

	userStore := store.NewMySQLUserStore(sqlDB)
	authService := auth.NewService(cfg.JWTSecret, cfg.AccessTokenTTL, cfg.RefreshTokenTTL, userStore)
	userService := user.NewService(userStore)

	router := gin.New()
	router.Use(gin.Logger(), gin.Recovery())

	router.GET("/healthz", func(c *gin.Context) {
		c.JSON(http.StatusOK, gin.H{"status": "ok"})
	})

	api.RegisterRoutes(router, authService, userService)

	log.Printf("login server listening on %s", cfg.Address)
	if err := router.Run(cfg.Address); err != nil {
		log.Fatalf("server exited with error: %v", err)
	}
}
