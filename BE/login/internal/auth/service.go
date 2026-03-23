package auth

import (
	"errors"
	"fmt"
	"time"

	"github.com/golang-jwt/jwt/v5"
	"golang.org/x/crypto/bcrypt"

	"login/internal/store"
)

const (
	tokenTypeAccess  = "access"
	tokenTypeRefresh = "refresh"
	issuer           = "login-server"
)

var ErrInvalidCredentials = errors.New("invalid credentials")

type Service struct {
	secret          []byte
	accessTokenTTL  time.Duration
	refreshTokenTTL time.Duration
	userStore       store.UserStore
	now             func() time.Time
}

type TokenPair struct {
	AccessToken      string
	RefreshToken     string
	AccessExpiresIn  int64
	RefreshExpiresIn int64
}

type Claims struct {
	TokenType string `json:"token_type"`
	jwt.RegisteredClaims
}

func NewService(secret string, accessTokenTTL, refreshTokenTTL time.Duration, userStore store.UserStore) *Service {
	return &Service{
		secret:          []byte(secret),
		accessTokenTTL:  accessTokenTTL,
		refreshTokenTTL: refreshTokenTTL,
		userStore:       userStore,
		now:             time.Now,
	}
}

func (s *Service) Authenticate(id, password string) (*TokenPair, error) {
	user, err := s.userStore.FindByID(id)
	if err != nil {
		return nil, ErrInvalidCredentials
	}

	if err := bcrypt.CompareHashAndPassword([]byte(user.PasswordHash), []byte(password)); err != nil {
		return nil, ErrInvalidCredentials
	}

	return s.generateTokenPair(user.ID)
}

func (s *Service) Refresh(refreshToken string) (string, int64, error) {
	claims, err := s.parseToken(refreshToken, tokenTypeRefresh)
	if err != nil {
		return "", 0, err
	}

	accessToken, expiresAt, err := s.generateToken(claims.Subject, tokenTypeAccess, s.accessTokenTTL)
	if err != nil {
		return "", 0, err
	}

	return accessToken, expiresAt.Unix(), nil
}

func (s *Service) generateTokenPair(userID string) (*TokenPair, error) {
	accessToken, accessExpiresAt, err := s.generateToken(userID, tokenTypeAccess, s.accessTokenTTL)
	if err != nil {
		return nil, err
	}

	refreshToken, refreshExpiresAt, err := s.generateToken(userID, tokenTypeRefresh, s.refreshTokenTTL)
	if err != nil {
		return nil, err
	}

	return &TokenPair{
		AccessToken:      accessToken,
		RefreshToken:     refreshToken,
		AccessExpiresIn:  accessExpiresAt.Unix(),
		RefreshExpiresIn: refreshExpiresAt.Unix(),
	}, nil
}

func (s *Service) generateToken(userID, tokenType string, ttl time.Duration) (string, time.Time, error) {
	now := s.now()
	expiresAt := now.Add(ttl)

	claims := Claims{
		TokenType: tokenType,
		RegisteredClaims: jwt.RegisteredClaims{
			Subject:   userID,
			Issuer:    issuer,
			IssuedAt:  jwt.NewNumericDate(now),
			ExpiresAt: jwt.NewNumericDate(expiresAt),
		},
	}

	token := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	signedToken, err := token.SignedString(s.secret)
	if err != nil {
		return "", time.Time{}, fmt.Errorf("sign token: %w", err)
	}

	return signedToken, expiresAt, nil
}

func (s *Service) parseToken(tokenString, expectedType string) (*Claims, error) {
	token, err := jwt.ParseWithClaims(
		tokenString,
		&Claims{},
		func(token *jwt.Token) (any, error) {
			if token.Method.Alg() != jwt.SigningMethodHS256.Alg() {
				return nil, fmt.Errorf("unexpected signing method: %s", token.Method.Alg())
			}
			return s.secret, nil
		},
		jwt.WithValidMethods([]string{jwt.SigningMethodHS256.Alg()}),
		jwt.WithIssuer(issuer),
	)
	if err != nil {
		return nil, err
	}

	claims, ok := token.Claims.(*Claims)
	if !ok || !token.Valid {
		return nil, errors.New("invalid token")
	}

	if claims.TokenType != expectedType {
		return nil, errors.New("invalid token type")
	}

	if claims.Subject == "" {
		return nil, errors.New("missing subject")
	}

	return claims, nil
}
