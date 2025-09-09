from django.shortcuts import render
from rest_framework.decorators import api_view, permission_classes,authentication_classes
from rest_framework.response import Response
from rest_framework import status
from django.db import transaction
from .serializers import TemperatureRecordSerializer
from django.http import JsonResponse

from rest_framework.permissions import AllowAny

from .models import TemperatureRecord


@api_view(["POST"])
def telemetry_temperature(request):
    data=request.data
    items = data.get("records") if isinstance(data,dict) and "records" in data else [data]
    ser=TemperatureRecordSerializer(data=items,many=True); ser.is_valid(raise_exception=True)
    objs=[TemperatureRecordSerializer.Meta.model(**row) for row in ser.validated_data]
    with transaction.atomic(): TemperatureRecordSerializer.Meta.model.objects.bulk_create(objs)
    return Response({"ok":True,"inserted":len(objs)},status=status.HTTP_201_CREATED)




@api_view(["GET"])
@authentication_classes([])         
@permission_classes([AllowAny]) 
def latest_temperatures(request):
    qs = TemperatureRecord.objects.order_by("-ts")[:10]
    return Response(TemperatureRecordSerializer(qs, many=True).data)


def home(request):
    return JsonResponse({
        "ok": True,
        "message": "Django is running.",
        "try": "POST JSON to /api/telemetry/temperature/",
        "docs": ["admin/"]
    })

