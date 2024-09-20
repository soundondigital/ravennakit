import numpy as np

int8 = np.array([-128, 127, 0, -127], dtype=np.int8)
print(f'int8 to int16: {int8} > {np.int16(int8) << 8}')

int16 = np.array([-32768, 32767, 0, -32767], dtype=np.int16)
print(f'int16 to int32: {int16} > {np.int32(int16) << 16}')

