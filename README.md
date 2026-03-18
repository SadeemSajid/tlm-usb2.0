# USB 2.0 TLM

## Transaction Types

- IN/READ: `IN` token packet sent from host to device. Device responds with one or more DATA packets. Host finally responds with a handshake packet. On a NAK from the device, the host retries the transaction.
