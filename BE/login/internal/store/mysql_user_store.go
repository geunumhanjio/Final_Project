package store

import (
	"database/sql"
	"errors"
	"fmt"
	"strings"

	mysql "github.com/go-sql-driver/mysql"
	"golang.org/x/crypto/bcrypt"
)

var (
	ErrUserNotFound     = errors.New("user not found")
	ErrUserAlreadyExist = errors.New("user already exists")
)

type MySQLUserStore struct {
	db *sql.DB
}

func NewMySQLUserStore(db *sql.DB) *MySQLUserStore {
	return &MySQLUserStore{db: db}
}

func (s *MySQLUserStore) FindByID(id string) (*User, error) {
	const query = `
		SELECT id, name, email, password_hash, created_at, updated_at
		FROM users
		WHERE id = ?
	`

	var user User
	err := s.db.QueryRow(query, id).Scan(
		&user.ID,
		&user.Name,
		&user.Email,
		&user.PasswordHash,
		&user.CreatedAt,
		&user.UpdatedAt,
	)
	if err != nil {
		if errors.Is(err, sql.ErrNoRows) {
			return nil, ErrUserNotFound
		}
		return nil, err
	}

	return &user, nil
}

func (s *MySQLUserStore) List() ([]User, error) {
	const query = `
		SELECT id, name, email, password_hash, created_at, updated_at
		FROM users
		ORDER BY created_at DESC
	`

	rows, err := s.db.Query(query)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var users []User
	for rows.Next() {
		var user User
		if err := rows.Scan(
			&user.ID,
			&user.Name,
			&user.Email,
			&user.PasswordHash,
			&user.CreatedAt,
			&user.UpdatedAt,
		); err != nil {
			return nil, err
		}

		users = append(users, user)
	}

	return users, rows.Err()
}

func (s *MySQLUserStore) Create(input CreateUserInput) (*User, error) {
	passwordHash, err := bcrypt.GenerateFromPassword([]byte(input.Password), bcrypt.DefaultCost)
	if err != nil {
		return nil, err
	}

	const query = `
		INSERT INTO users (id, name, email, password_hash)
		VALUES (?, ?, ?, ?)
	`

	_, err = s.db.Exec(query, input.ID, input.Name, input.Email, string(passwordHash))
	if err != nil {
		if isDuplicateEntry(err) {
			return nil, ErrUserAlreadyExist
		}
		return nil, err
	}

	return s.FindByID(input.ID)
}

func (s *MySQLUserStore) Update(id string, input UpdateUserInput) (*User, error) {
	updates := make([]string, 0, 3)
	args := make([]any, 0, 4)

	if input.Name != nil {
		updates = append(updates, "name = ?")
		args = append(args, *input.Name)
	}
	if input.Email != nil {
		updates = append(updates, "email = ?")
		args = append(args, *input.Email)
	}
	if input.Password != nil {
		passwordHash, err := bcrypt.GenerateFromPassword([]byte(*input.Password), bcrypt.DefaultCost)
		if err != nil {
			return nil, err
		}
		updates = append(updates, "password_hash = ?")
		args = append(args, string(passwordHash))
	}

	if len(updates) == 0 {
		return s.FindByID(id)
	}

	query := fmt.Sprintf(`
		UPDATE users
		SET %s, updated_at = CURRENT_TIMESTAMP
		WHERE id = ?
	`, strings.Join(updates, ", "))
	args = append(args, id)

	result, err := s.db.Exec(query, args...)
	if err != nil {
		if isDuplicateEntry(err) {
			return nil, ErrUserAlreadyExist
		}
		return nil, err
	}

	rowsAffected, err := result.RowsAffected()
	if err != nil {
		return nil, err
	}
	if rowsAffected == 0 {
		return nil, ErrUserNotFound
	}

	return s.FindByID(id)
}

func (s *MySQLUserStore) Delete(id string) error {
	const query = `DELETE FROM users WHERE id = ?`

	result, err := s.db.Exec(query, id)
	if err != nil {
		return err
	}

	rowsAffected, err := result.RowsAffected()
	if err != nil {
		return err
	}
	if rowsAffected == 0 {
		return ErrUserNotFound
	}

	return nil
}

func isDuplicateEntry(err error) bool {
	if err == nil {
		return false
	}

	var mysqlErr *mysql.MySQLError
	if errors.As(err, &mysqlErr) {
		return mysqlErr.Number == 1062
	}

	return false
}
