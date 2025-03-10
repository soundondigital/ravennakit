# Glossary

## Audio

### Sample

A single value of a sound wave at a particular point in time for a particular channel.

### Frame

A collection of samples at a particular point in time for all channels. The number of samples in a frame is equal to the
number of channels.

### Block

A collection of frames. The number of frames in a block is equal to the block size.

### Sample rate

The number of samples per second, the number of times an audio signal is sampled per second.

## RTP

### Session

An RTP session is defined as a context for the exchange of RTP packets, typically identified by:

1. A transport address pair (sender's IP/port and receiver's IP/port).
2. A unique SSRC namespace, where SSRCs are unique within the session.

### SSRC

SSRCs are unique to each stream within a session (but practically globally). They are used to identify the source of a
stream.
