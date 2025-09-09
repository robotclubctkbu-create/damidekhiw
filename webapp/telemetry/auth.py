import os
from django.utils.translation import gettext_lazy as _
from rest_framework import authentication, exceptions

class StaticTokenUser:
    """Minimal user object that DRF considers authenticated."""
    def __init__(self, username="ingest"):
        self.username = username
    @property
    def is_authenticated(self):
        return True

class BearerTokenAuthentication(authentication.BaseAuthentication):
    keyword = "Bearer"

    def authenticate(self, request):
        auth = authentication.get_authorization_header(request).split()
        if not auth or auth[0].lower() != b"bearer":
            raise exceptions.AuthenticationFailed(_("Missing bearer"))

        if len(auth) == 1:
            raise exceptions.AuthenticationFailed(_("Invalid bearer header"))

        token = auth[1].decode()
        expected = os.getenv("API_TOKEN", "dev-token")
        if token != expected:
            raise exceptions.AuthenticationFailed(_("Invalid token"))

        # Return an authenticated user object + token
        return (StaticTokenUser(username="ingest"), token)
