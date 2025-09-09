from rest_framework import serializers
from .models import TemperatureRecord
class TemperatureRecordSerializer(serializers.ModelSerializer):
    class Meta: model=TemperatureRecord; fields="__all__"
