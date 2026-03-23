package user

import (
	"errors"
	"strings"

	"login/internal/store"
)

var ErrInvalidUserInput = errors.New("invalid user input")

type Service struct {
	userStore store.UserStore
}

func NewService(userStore store.UserStore) *Service {
	return &Service{userStore: userStore}
}

func (s *Service) ListUsers() ([]store.User, error) {
	return s.userStore.List()
}

func (s *Service) GetUser(id string) (*store.User, error) {
	return s.userStore.FindByID(strings.TrimSpace(id))
}

func (s *Service) CreateUser(input store.CreateUserInput) (*store.User, error) {
	input.ID = strings.TrimSpace(input.ID)
	input.Name = strings.TrimSpace(input.Name)
	input.Email = strings.TrimSpace(input.Email)

	if input.ID == "" || input.Password == "" || input.Email == "" {
		return nil, ErrInvalidUserInput
	}

	return s.userStore.Create(input)
}

func (s *Service) UpdateUser(id string, input store.UpdateUserInput) (*store.User, error) {
	id = strings.TrimSpace(id)
	if id == "" {
		return nil, ErrInvalidUserInput
	}

	if input.Name != nil {
		name := strings.TrimSpace(*input.Name)
		input.Name = &name
	}
	if input.Email != nil {
		email := strings.TrimSpace(*input.Email)
		if email == "" {
			return nil, ErrInvalidUserInput
		}
		input.Email = &email
	}
	if input.Password != nil {
		password := strings.TrimSpace(*input.Password)
		if password == "" {
			return nil, ErrInvalidUserInput
		}
		input.Password = &password
	}

	return s.userStore.Update(id, input)
}

func (s *Service) DeleteUser(id string) error {
	id = strings.TrimSpace(id)
	if id == "" {
		return ErrInvalidUserInput
	}

	return s.userStore.Delete(id)
}
