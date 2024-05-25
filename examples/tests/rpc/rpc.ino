


const char* post = R""""(
POST /SwitchPower/Control HTTP/1.1
HOST: 192.168.1.35 43365
CONTENT-TYPE: text/xml; charset="utf-8"
SOAPACTION: "urn:schemas-upnp-org:service:SwitchPower:1#SetTarget"

<?xml version="1.0"?>
<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/"
s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
<s:Body>
<u:SetTarget xmlns:u="urn:schemas-upnp-org:service:SwitchPower:1">
<newTargetValue>TRUE</newTargetValue>
</u:SetTarget>
</s:Body>
</s:Envelope>

)""";


<?xml version="1.0"?>
<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
<s:Body>
<u:SetTargetResponse xmlns:u="urn:schemas-upnp-org:service:SwitchPower:1">
</u:SetTargetResponse>
</s:Body>
</s:Envelope>