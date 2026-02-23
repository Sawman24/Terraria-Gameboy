import os
import struct
from PIL import Image

def read_xnb(filepath):
    with open(filepath, 'rb') as f:
        # Header
        header = f.read(3)
        if header != b'XNB':
            print(f"Not an XNB file: {filepath}")
            return None
        
        platform = f.read(1)
        version = struct.unpack('B', f.read(1))[0]
        flags = struct.unpack('B', f.read(1))[0]
        compressed = flags & 0x01
        
        file_size = struct.unpack('<I', f.read(4))[0]
        
        if compressed:
            print(f"File is compressed, compression not supported in this simple script: {filepath}")
            return None
        
        # Reader count
        reader_count = read_7bit_encoded_int(f)
        readers = []
        for _ in range(reader_count):
            reader_name = read_string(f)
            reader_version = struct.unpack('<I', f.read(4))[0]
            readers.append(reader_name)
        
        # Shared resource count
        shared_resource_count = read_7bit_encoded_int(f)
        
        # The actual object
        # Type ID (7-bit encoded int)
        type_id = read_7bit_encoded_int(f)
        
        # For Texture2DReader, we expect certain data
        # Surface format
        surface_format = struct.unpack('<I', f.read(4))[0]
        # Width
        width = struct.unpack('<I', f.read(4))[0]
        # Height
        height = struct.unpack('<I', f.read(4))[0]
        # Mip count
        mip_count = struct.unpack('<I', f.read(4))[0]
        # Data size
        data_size = struct.unpack('<I', f.read(4))[0]
        
        pixel_data = f.read(data_size)
        
        return {
            'format': surface_format,
            'width': width,
            'height': height,
            'data': pixel_data
        }

def read_7bit_encoded_int(f):
    result = 0
    bits_read = 0
    while True:
        value = struct.unpack('B', f.read(1))[0]
        result |= (value & 0x7F) << bits_read
        bits_read += 7
        if (value & 0x80) == 0:
            break
    return result

def read_string(f):
    length = read_7bit_encoded_int(f)
    return f.read(length).decode('utf-8')

def convert_to_png(xnb_data, output_path):
    if xnb_data['format'] == 0: # SurfaceFormat.Color (RGBA)
        # XNA SurfaceFormat.Color is actually RGBA or BGRA? 
        # Usually it's RGBA in XNA 4.0 for Texture2D.
        # However, Terraria might be BGRA.
        img = Image.frombytes('RGBA', (xnb_data['width'], xnb_data['height']), xnb_data['data'])
        
        # If the colors look wrong (blue skin), we might need to swap R and B.
        # b, g, r, a = img.split()
        # img = Image.merge('RGBA', (r, g, b, a))
        
        img.save(output_path)
        return True
    else:
        print(f"Unsupported surface format: {xnb_data['format']}")
        return False

def process_directory(input_dir, output_dir):
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    
    for filename in os.listdir(input_dir):
        if filename.endswith('.xnb'):
            input_path = os.path.join(input_dir, filename)
            output_path = os.path.join(output_dir, filename.replace('.xnb', '.png'))
            
            print(f"Processing {filename}...")
            try:
                xnb_data = read_xnb(input_path)
                if xnb_data:
                    convert_to_png(xnb_data, output_path)
            except Exception as e:
                print(f"Error processing {filename}: {e}")

if __name__ == '__main__':
    input_folder = r'c:\Users\sawye\Desktop\Terraria Beta\Terraria Beta\Content\Images'
    output_folder = r'c:\Users\sawye\Desktop\Terraria Beta\DumpedAssets'
    process_directory(input_folder, output_folder)
