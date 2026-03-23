package db

import (
	"database/sql"
	"fmt"
	"strings"
	"time"

	_ "github.com/go-sql-driver/mysql"

	"login/internal/config"
)

func OpenAndMigrate(cfg *config.Config) (*sql.DB, error) {
	serverDB, err := sql.Open("mysql", cfg.MySQLServerDSN())
	if err != nil {
		return nil, err
	}
	defer serverDB.Close()

	if err := serverDB.Ping(); err != nil {
		return nil, fmt.Errorf("ping mysql server: %w", err)
	}

	if _, err := serverDB.Exec("CREATE DATABASE IF NOT EXISTS `" + escapeIdentifier(cfg.MySQLDatabase) + "` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci"); err != nil {
		return nil, fmt.Errorf("create database: %w", err)
	}

	appDB, err := sql.Open("mysql", cfg.MySQLDatabaseDSN())
	if err != nil {
		return nil, err
	}

	appDB.SetMaxOpenConns(10)
	appDB.SetMaxIdleConns(5)
	appDB.SetConnMaxLifetime(5 * time.Minute)

	if err := appDB.Ping(); err != nil {
		appDB.Close()
		return nil, fmt.Errorf("ping mysql database: %w", err)
	}

	if err := migrate(appDB); err != nil {
		appDB.Close()
		return nil, err
	}

	return appDB, nil
}

func migrate(db *sql.DB) error {
	const createUsersTable = `
		CREATE TABLE IF NOT EXISTS users (
			id VARCHAR(64) NOT NULL PRIMARY KEY,
			name VARCHAR(128) NOT NULL DEFAULT '',
			email VARCHAR(255) NOT NULL UNIQUE,
			password_hash VARCHAR(255) NOT NULL,
			created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
			updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
		) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
	`

	_, err := db.Exec(createUsersTable)
	if err != nil {
		return fmt.Errorf("create users table: %w", err)
	}

	return nil
}

func escapeIdentifier(value string) string {
	return strings.ReplaceAll(value, "`", "``")
}
