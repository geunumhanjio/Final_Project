package store

import "time"

type User struct {
	ID           string    `json:"id"`
	Name         string    `json:"name"`
	Email        string    `json:"email"`
	PasswordHash string    `json:"-"`
	CreatedAt    time.Time `json:"created_at"`
	UpdatedAt    time.Time `json:"updated_at"`
}

type CreateUserInput struct {
	ID       string
	Name     string
	Email    string
	Password string
}

type UpdateUserInput struct {
	Name     *string
	Email    *string
	Password *string
}

type UserStore interface {
	FindByID(id string) (*User, error)
	List() ([]User, error)
	Create(input CreateUserInput) (*User, error)
	Update(id string, input UpdateUserInput) (*User, error)
	Delete(id string) error
}
