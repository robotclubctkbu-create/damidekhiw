from django.db import models
class TemperatureRecord(models.Model):
    mac_master=models.CharField(max_length=32)
    mac_slave=models.CharField(max_length=32)
    value=models.FloatField()
    unit=models.CharField(max_length=8,default="C")
    ts=models.BigIntegerField()
    rssi=models.IntegerField(null=True,blank=True)
    page=models.IntegerField(null=True,blank=True)
    firmware=models.CharField(max_length=64,null=True,blank=True)
    created_at=models.DateTimeField(auto_now_add=True)
