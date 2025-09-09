from django.urls import path
from .views import telemetry_temperature, home,latest_temperatures

urlpatterns = [
    path("", home, name="home"),  # <- handles "/"
    path("api/telemetry/temperature/", telemetry_temperature, name="telemetry-temp"),
    path("api/telemetry/temperature",  telemetry_temperature),
    path("api/telemetry/latest/", latest_temperatures, name="telemetry-latest"),
]

