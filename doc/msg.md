## Configuration file

An example configuration file:
```
{
  "name": "donkey1",
  "UID": "895636d5-0a65-4dea-8157-91acf7eb02d2",
  "type": "donkey",
  "capabilities": ["charge", "moveto"],
  "local_endpoint": "ipc:///tmp/donkey1-local",
  "gossip_endpoint": "ipc:///tmp/local-hub",
  "msg_filter_length": 5000,
  "resend_interval": 500
}
```
* name: name used to display human readable things. Does not have to be unique!
* UID: a unique ID that is used by the software. Must be unique! Reommendation: Set zyre UUID of component after start.
* type: the type of the component
* capabilities: available capabilities of this component
* local_endpoint: local endpoint used by zyre's gossip protocol
* gossip_endpoint: shared gossip endpoint used by zyre's gossip protocol
* msg_filter_length: length in msec how long msgs are kept in memory for avoiding receiving same msg multiple times. 
* resend_interval: time on msec after which msg will be resent

## Envelope structure

- metamodel: STRING
- model: STRING
- type: STRING
- payload: JSON subpart

## Payload structure
Here all msgs are listed with their type and payload structure.

### Type: send_request
This is the message that needs to be sent to the mediator to request sending its payload.
```
{
  UID: 07a65b9b-ecfc-46f4-9997-a2619380857a,
  requester: a1279775-99d1-4480-aded-05985fc9641e,
  recipients: [88aad5c8-9e4e-494f-bdf5-f8f0049678e1,fe24d4cf-80fb-4e77-b04e-7f697d66fcb0],
  timeout: 5000,
  payload_type: RSG_update,
  payload: {...}
}
```
* UID: UID of message that is used in communication back to requester (see communication report). Needs to be unique for requester but not globally unique.
* requester: UID of requesting component
* recipients: list of recipients UIDs. Can be empty. Payload is always broadcasted, but all recipients in this list are expected to send an acknowledgment upon reception. Otherwise, payload is periodically resent until all recipeints have acknowledged reception or timeout occurs.
* timeout: time in msec after which periodic resending will be aborted
* payload_type: defines the type of payload similar to type of the envelope. Currently not used but will be used to enable transmission of binary data later.
* payload: JSON object that will be sent

### Type: communication_ack
This is the acknowledgment that is sent by a receiving communication mediator to the sending mediator.
```
{
  UID: 07a65b9b-ecfc-46f4-9997-a2619380857a,
  ID_receiver: fe24d4cf-80fb-4e77-b04e-7f697d66fcb0
}
```
* UID: UID of the message that is acknowledged
* ID_receiver: ID of the receiver that sends the acknowledgement

### Type: communication_report
The report that is sent from the communication mediator to the coponent that requested to send data.
```
{
  UID: 07a65b9b-ecfc-46f4-9997-a2619380857a,
  success: true,
  error: none,
  recipients_delivered: [88aad5c8-9e4e-494f-bdf5-f8f0049678e1,fe24d4cf-80fb-4e77-b04e-7f697d66fcb0],
  recipients_undelivered: []
}
```
* UID: UID of the message that was delivered
* success: true or false, depending on outcome
* error: string describing the outcome: [none|Timeout|Unknown recipients]
* recipients_delivered: list of recipients' UIDs to which msg was delivered
* recipients_undelivered: list of recipients' UIDs to which msg could not be delivered (or from which no acknowledgement has been received).
If the list of recipients contained unknown recipients, the undelivered list contains the unknown recipients.

### Type: query_remote_peer_list
Returns a list containing all peers.
Request message:
```
{
  UID: 2147aba0-0d59-41ec-8531-f6787fe52b60
}
```
Return message: Type: peer-list
```
{
  UID: 2147aba0-0d59-41ec-8531-f6787fe52b60,
  peer_list: [
    {
      peerid: 48d9aaf2-22ca-44fd-b4f5-5e67023c6dcb,
      {header content (key-value pairs)}
    },
    {
      peerid: a1279775-99d1-4480-aded-05985fc9641e,
      {header content (key-value pairs)}
    },
    ...
  ]
}
```
* UID: UID of message that is used in communication back to requester. Needs to be unique for requester but not globally unique.
* peer_list: array containing the UUIDs of all connected peers together with their header content

### Type: query_remote_file
Fetch a remote file, store it locally, and return local file path.
Request message:
```
{
  UID: 2147aba0-0d59-41ec-8531-f6787fe52b60,
  URI: peer_id:/path/filename
}
```
Return message to requesting mediator: Type: endpoint
Return to setup the P2P connection for downloading the file.
```
{
  UID: 2147aba0-0d59-41ec-8531-f6787fe52b60,
  URI: tcp://host:port
}
```
Return message to remote mediator to file is downloaded: Type: remote_file_done
```
{
  UID: 2147aba0-0d59-41ec-8531-f6787fe52b60,
}
```
Return message to local component: Type: file_path
Return the filepath where the file can be accessed locally.
```
{
  UID: 2147aba0-0d59-41ec-8531-f6787fe52b60,
  URI: local_path/filename
}
```
* UID: UID of message that is used in communication back to requester. Needs to be unique for requester but not globally unique.
* URI: needs to contain the peer_id from which the file can be downloaded and the file path at which the file is stored (git this URI from the SWM)
* file_path: the path where the file has been stored locally
