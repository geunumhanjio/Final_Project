package config

import (
	"errors"
	"fmt"
	"os"
	"strings"
	"time"
)

type Config struct {
	Address         string
	JWTSecret       string
	AccessTokenTTL  time.Duration
	RefreshTokenTTL time.Duration
	MySQLHost       string
	MySQLPort       string
	MySQLUser       string
	MySQLPassword   string
	MySQLDatabase   string
}

func Load() (*Config, error) {
	jwtSecret, err := readSecret()
	if err != nil {
		return nil, err
	}

	accessTTL, err := readDuration("ACCESS_TOKEN_TTL", "1h")
	if err != nil {
		return nil, err
	}

	refreshTTL, err := readDuration("REFRESH_TOKEN_TTL", "336h")
	if err != nil {
		return nil, err
	}

	return &Config{
		Address:         readString("LOGIN_SERVER_ADDR", ":8080"),
		JWTSecret:       jwtSecret,
		AccessTokenTTL:  accessTTL,
		RefreshTokenTTL: refreshTTL,
		MySQLHost:       readString("MYSQL_HOST", "127.0.0.1"),
		MySQLPort:       readString("MYSQL_PORT", "3306"),
		MySQLUser:       readString("MYSQL_USER", "rokgeun"),
		MySQLPassword:   readString("MYSQL_PASSWORD", "23audrms"),
		MySQLDatabase:   readString("MYSQL_DATABASE", "login_server"),
	}, nil
}

func (c *Config) MySQLServerDSN() string {
	return c.MySQLUser + ":" + c.MySQLPassword + "@tcp(" + c.MySQLHost + ":" + c.MySQLPort + ")/?parseTime=true&charset=utf8mb4"
}

func (c *Config) MySQLDatabaseDSN() string {
	return c.MySQLUser + ":" + c.MySQLPassword + "@tcp(" + c.MySQLHost + ":" + c.MySQLPort + ")/" + c.MySQLDatabase + "?parseTime=true&charset=utf8mb4"
}

func readString(key, fallback string) string {
	value := os.Getenv(key)
	if value == "" {
		return fallback
	}
	return value
}

func readDuration(key, fallback string) (time.Duration, error) {
	value := readString(key, fallback)
	duration, err := time.ParseDuration(value)
	if err != nil {
		return 0, fmt.Errorf("%s is invalid: %w", key, err)
	}
	return duration, nil
}

func readSecret() (string, error) {
	secretFile := os.Getenv("JWT_SECRET_FILE")
	if secretFile != "" {
		content, err := os.ReadFile(secretFile)
		if err != nil {
			return "", fmt.Errorf("failed to read JWT_SECRET_FILE: %w", err)
		}

		secret := strings.TrimSpace(string(content))
		if secret == "" {
			return "", errors.New("JWT_SECRET_FILE is empty")
		}
		return secret, nil
	}

	secret := strings.TrimSpace(os.Getenv("JWT_SECRET"))
	if secret == "" {
		return "", errors.New("JWT_SECRET_FILE or JWT_SECRET is required")
	}

	return secret, nil
}
