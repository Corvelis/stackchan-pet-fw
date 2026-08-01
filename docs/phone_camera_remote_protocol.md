# StopWatch Phone Camera Remote Protocol

## Scope

This feature is available only on M5Stack StopWatch. It does not add a camera
to the StopWatch; it sends shutter requests to a connected companion app.

Clients can detect support through this `device.info.capabilities` value:

```json
[
  "phone_camera.remote_shutter.v1",
  "phone_camera.remote_lens.v1"
]
```

This protocol is independent of the CoreS3 `camera_button` event, HTTP
`POST /capture`, and USB Serial `capture.request`.

## Transport

The same JSON messages can travel over WebSocket or USB Serial. The transport
that sends `ready:true` becomes the shutter-request destination. The firmware
never broadcasts one shutter request to both transports.

The transport is frozen while a request is pending. `ready:true` and results
from another transport are ignored during that request.

## Ready State

The app sends this after its camera screen is open and ready to capture:

```json
{
  "type": "phone_camera.state",
  "version": 1,
  "available": true,
  "ready": true,
  "lens": "back",
  "supportedLenses": ["front", "back"]
}
```

`lens` is the camera currently selected by the app: `front` is the front-facing
camera and `back` is the rear-facing camera. The StopWatch treats the app as
authoritative and shows `IN` or `OUT` on the camera overlay. Long-press switching
is enabled only when `supportedLenses` contains both values.

It sends this on the same transport when its camera becomes unavailable:

```json
{"type":"phone_camera.state","version":1,"available":false,"ready":false}
```

`ready:false`, disconnection of the owning transport, or turning the StopWatch
display off clears any pending request and returns the feature to unavailable.
The firmware does not automatically fall back to another transport.

## Shutter Request

Pressing the lower-right camera button sends this only to the ready transport:

```json
{
  "type": "phone_camera.shutter.request",
  "version": 1,
  "requestId": "pcam-a31f9270-00000017",
  "mode": "photo"
}
```

The request ID combines a boot-session token with a monotonically increasing
sequence. Apps should deduplicate requests by `requestId`.

## Lens Change

When no camera operation is pending, holding the lower-right camera button for
about 0.8 seconds sends an explicit request for the other supported lens. It is
sent only on the transport that reported readiness.

```json
{
  "type": "phone_camera.lens.set.request",
  "version": 1,
  "requestId": "lens-a31f9270-00000018",
  "lens": "front"
}
```

Success:

```json
{
  "type": "phone_camera.lens.set.result",
  "version": 1,
  "requestId": "lens-a31f9270-00000018",
  "status": "applied",
  "lens": "front"
}
```

Failure:

```json
{
  "type": "phone_camera.lens.set.result",
  "version": 1,
  "requestId": "lens-a31f9270-00000018",
  "status": "camera_unavailable",
  "lens": "back"
}
```

As with shutter results, the firmware accepts a lens result only when its
operation, transport, and `requestId` all match the pending request. Success
requires `status:applied` and a returned `lens` matching the requested value.
Lens changes time out after 10 seconds.

Shutter and lens operations are mutually exclusive. The app should persist the
applied lens and use it for later captures and subsequent
`phone_camera.state` messages.

## Result

Success:

```json
{
  "type": "phone_camera.shutter.result",
  "version": 1,
  "requestId": "pcam-a31f9270-00000017",
  "status": "captured"
}
```

Failure:

```json
{
  "type": "phone_camera.shutter.result",
  "version": 1,
  "requestId": "pcam-a31f9270-00000017",
  "status": "capture_failed",
  "error": "camera_not_ready"
}
```

The firmware accepts a result only while a request is pending and only when
both its `requestId` and transport match the request. Other text, binary, PCM,
IDs, and transports do not clear the pending state. A non-empty `status` is
terminal: `captured` means success and any other value means failure.

A request times out after 30 seconds. Success or failure feedback remains
visible for about 0.9 seconds before returning to ready, unless the owning
transport becomes unavailable first.
