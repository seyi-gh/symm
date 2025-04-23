### WebSocket Server

#### Methods
`bool` `| decode_frame |` method decode the encrypted frame (payload) from the client.
<br> The payload is the packet used for handshakes and communication HTTP/1.1<br>
  ##### Logic (basic algorithm functionality)
    1. Initial len (condition)
      - Check if the length of the frame is less than 2, this is because every frame
      must have at least 2 bytes and check if the buffer (complete frame) is not null.
        First byte - FIN bit and opcode
        Second byte - mask bit and payload length (1)
      - If the length is too short or the buffer is null an error is logged (0)
    2. Extracting the first two bytes (process)
      - FIN bit - Identificator of last byte
      - Opcode - Type of frame (text, binary, etc)
      - Mask bit - Indicating if the payload is masked and the length of it
    3. Opcode validation (condition)
      - Extract bitwise AND operation from byte1 (byte1 & 0x0F). Then if the opcode
      is 0x1 (text frame) an error is logged (0)
      - if is other method return true (1)
    4. Determining the payload length (condition)
      - From the 8 bits of the 2 byte
      - The fist of it is if the frame is masked or not
      - The other 7 are the confirmation of the length of the payload, with the
      operation (byte2 & 0x7F) will know the length, if it is 126. It means that
      the payload length is stored in the next 2 bytes (16 bit). If the payload
      length is 127 that means that the actual payload length is stored in the
      next 8 bytes (64 bit), after getting the payload length and the offset
      they are stored. (1)
      - If the frame does not contain enough bytes for the extender payload
      length, an error is logged, and the method returns false (0)
    5. Validation of the frame length (condition)
      - The method checks if the frame containers enough bytes for the payload
      and the 4 bytes masking key. (1)
      - If not an error is logged and returning false (0)
    6. Extracting the masking key (process)
      - The masking key is a 4 byte value used to mask (encrypt) the payload. It
      is extracted from the frame starting (after the offset).
    7. Unmasking the payload (process)
      - The payload is unmasked by XORing (process method for bytes), going for each
      byte of the payload with the mask key, in a bucle to the end. The unmasked
      payload is stored.
    8. End of method
      - If the payload is unmasked correctly then return true (1)

`bool` ` perform_handshake `
  ##### Logic