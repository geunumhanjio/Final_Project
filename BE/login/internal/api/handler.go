package api

import (
	"errors"
	"net/http"

	"github.com/gin-gonic/gin"

	"login/internal/auth"
	"login/internal/store"
	"login/internal/user"
)

type Handler struct {
	authService *auth.Service
	userService *user.Service
}

type loginRequest struct {
	ID       string `json:"id" binding:"required"`
	Password string `json:"password" binding:"required"`
}

type refreshRequest struct {
	RefreshToken string `json:"refresh_token" binding:"required"`
}

type createUserRequest struct {
	ID       string `json:"id" binding:"required"`
	Name     string `json:"name"`
	Email    string `json:"email" binding:"required,email"`
	Password string `json:"password" binding:"required"`
}

type updateUserRequest struct {
	Name     *string `json:"name"`
	Email    *string `json:"email"`
	Password *string `json:"password"`
}

func RegisterRoutes(router gin.IRouter, authService *auth.Service, userService *user.Service) {
	handler := &Handler{
		authService: authService,
		userService: userService,
	}

	router.POST("/login", handler.login)
	router.POST("/refresh", handler.refresh)
	router.GET("/users", handler.listUsers)
	router.GET("/users/:id", handler.getUser)
	router.POST("/users", handler.createUser)
	router.PUT("/users/:id", handler.updateUser)
	router.DELETE("/users/:id", handler.deleteUser)
}

func (h *Handler) login(c *gin.Context) {
	var req loginRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "invalid_request",
			"message": err.Error(),
		})
		return
	}

	tokenPair, err := h.authService.Authenticate(req.ID, req.Password)
	if err != nil {
		if errors.Is(err, auth.ErrInvalidCredentials) {
			c.JSON(http.StatusUnauthorized, gin.H{
				"error":   "invalid_credentials",
				"message": "id or password is incorrect",
			})
			return
		}

		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "internal_error",
			"message": "failed to issue tokens",
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"access_token":       tokenPair.AccessToken,
		"refresh_token":      tokenPair.RefreshToken,
		"token_type":         "Bearer",
		"access_expires_at":  tokenPair.AccessExpiresIn,
		"refresh_expires_at": tokenPair.RefreshExpiresIn,
	})
}

func (h *Handler) refresh(c *gin.Context) {
	var req refreshRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "invalid_request",
			"message": err.Error(),
		})
		return
	}

	accessToken, expiresAt, err := h.authService.Refresh(req.RefreshToken)
	if err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{
			"error":   "invalid_refresh_token",
			"message": "refresh token is invalid or expired",
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"access_token":      accessToken,
		"token_type":        "Bearer",
		"access_expires_at": expiresAt,
	})
}

func (h *Handler) listUsers(c *gin.Context) {
	users, err := h.userService.ListUsers()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "internal_error",
			"message": "failed to list users",
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{"users": users})
}

func (h *Handler) getUser(c *gin.Context) {
	userRecord, err := h.userService.GetUser(c.Param("id"))
	if err != nil {
		if errors.Is(err, store.ErrUserNotFound) {
			c.JSON(http.StatusNotFound, gin.H{
				"error":   "user_not_found",
				"message": "user does not exist",
			})
			return
		}

		c.JSON(http.StatusInternalServerError, gin.H{
			"error":   "internal_error",
			"message": "failed to fetch user",
		})
		return
	}

	c.JSON(http.StatusOK, userRecord)
}

func (h *Handler) createUser(c *gin.Context) {
	var req createUserRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "invalid_request",
			"message": err.Error(),
		})
		return
	}

	userRecord, err := h.userService.CreateUser(store.CreateUserInput{
		ID:       req.ID,
		Name:     req.Name,
		Email:    req.Email,
		Password: req.Password,
	})
	if err != nil {
		switch {
		case errors.Is(err, user.ErrInvalidUserInput):
			c.JSON(http.StatusBadRequest, gin.H{
				"error":   "invalid_user_input",
				"message": "id, email and password are required",
			})
		case errors.Is(err, store.ErrUserAlreadyExist):
			c.JSON(http.StatusConflict, gin.H{
				"error":   "user_already_exists",
				"message": "id or email is already in use",
			})
		default:
			c.JSON(http.StatusInternalServerError, gin.H{
				"error":   "internal_error",
				"message": "failed to create user",
			})
		}
		return
	}

	c.JSON(http.StatusCreated, userRecord)
}

func (h *Handler) updateUser(c *gin.Context) {
	var req updateUserRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"error":   "invalid_request",
			"message": err.Error(),
		})
		return
	}

	userRecord, err := h.userService.UpdateUser(c.Param("id"), store.UpdateUserInput{
		Name:     req.Name,
		Email:    req.Email,
		Password: req.Password,
	})
	if err != nil {
		switch {
		case errors.Is(err, user.ErrInvalidUserInput):
			c.JSON(http.StatusBadRequest, gin.H{
				"error":   "invalid_user_input",
				"message": "user id and update values must be valid",
			})
		case errors.Is(err, store.ErrUserNotFound):
			c.JSON(http.StatusNotFound, gin.H{
				"error":   "user_not_found",
				"message": "user does not exist",
			})
		case errors.Is(err, store.ErrUserAlreadyExist):
			c.JSON(http.StatusConflict, gin.H{
				"error":   "user_already_exists",
				"message": "email is already in use",
			})
		default:
			c.JSON(http.StatusInternalServerError, gin.H{
				"error":   "internal_error",
				"message": "failed to update user",
			})
		}
		return
	}

	c.JSON(http.StatusOK, userRecord)
}

func (h *Handler) deleteUser(c *gin.Context) {
	err := h.userService.DeleteUser(c.Param("id"))
	if err != nil {
		switch {
		case errors.Is(err, user.ErrInvalidUserInput):
			c.JSON(http.StatusBadRequest, gin.H{
				"error":   "invalid_user_input",
				"message": "user id is required",
			})
		case errors.Is(err, store.ErrUserNotFound):
			c.JSON(http.StatusNotFound, gin.H{
				"error":   "user_not_found",
				"message": "user does not exist",
			})
		default:
			c.JSON(http.StatusInternalServerError, gin.H{
				"error":   "internal_error",
				"message": "failed to delete user",
			})
		}
		return
	}

	c.Status(http.StatusNoContent)
}
