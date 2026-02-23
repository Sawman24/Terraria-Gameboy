import com.github.antag99.textract.extract.LzxDecoder;
import com.github.antag99.textract.extract.Xnb;

import java.awt.image.BufferedImage;
import java.io.*;
import java.nio.*;
import java.nio.charset.Charset;
import java.nio.file.*;
import javax.imageio.ImageIO;

public class XnbDump {

    static final int HEADER_SIZE = 14;

    public static void main(String[] args) throws Exception {
        String inputDir = "Terraria Beta\\Content\\Images";
        String outputDir = "DumpedAssets";

        File outDir = new File(outputDir);
        if (!outDir.exists()) outDir.mkdirs();

        // Also create subdirs for organization
        new File(outDir, "Items").mkdirs();
        new File(outDir, "NPCs").mkdirs();
        new File(outDir, "Tiles").mkdirs();
        new File(outDir, "Walls").mkdirs();
        new File(outDir, "Gore").mkdirs();
        new File(outDir, "Projectiles").mkdirs();
        new File(outDir, "Player").mkdirs();
        new File(outDir, "Backgrounds").mkdirs();
        new File(outDir, "Misc").mkdirs();
        new File(outDir, "Sounds").mkdirs();

        File inDir = new File(inputDir);
        File[] files = inDir.listFiles((d, name) -> name.endsWith(".xnb"));

        if (files == null || files.length == 0) {
            System.out.println("No XNB files found in " + inputDir);
            return;
        }

        LzxDecoder lzxDecoder = new LzxDecoder();
        int success = 0, fail = 0, skip = 0;

        for (File f : files) {
            try {
                extractFile(f, outDir, lzxDecoder);
                success++;
            } catch (Exception e) {
                if (e.getMessage() != null && e.getMessage().contains("skipping")) {
                    skip++;
                    System.out.println("  Skipped: " + e.getMessage());
                } else {
                    fail++;
                    System.out.println("  ERROR on " + f.getName() + ": " + e.getMessage());
                }
            }
        }

        // Also extract sounds
        File soundDir = new File("Terraria Beta\\Content\\Sounds");
        if (soundDir.exists()) {
            File[] soundFiles = soundDir.listFiles((d, name) -> name.endsWith(".xnb"));
            if (soundFiles != null) {
                for (File f : soundFiles) {
                    try {
                        extractFile(f, outDir, lzxDecoder);
                        success++;
                    } catch (Exception e) {
                        if (e.getMessage() != null && e.getMessage().contains("skipping")) {
                            skip++;
                        } else {
                            fail++;
                            System.out.println("  ERROR on " + f.getName() + ": " + e.getMessage());
                        }
                    }
                }
            }
        }

        System.out.println("\n=== DONE ===");
        System.out.println("Success: " + success);
        System.out.println("Skipped (fonts/effects): " + skip);
        System.out.println("Failed: " + fail);
    }

    static void extractFile(File inputFile, File outputDir, LzxDecoder lzxDecoder) throws Exception {
        byte[] fileBytes = Files.readAllBytes(inputFile.toPath());
        ByteBuffer buffer = ByteBuffer.wrap(fileBytes);
        buffer.order(ByteOrder.LITTLE_ENDIAN);

        // Check XNB header
        if (buffer.get() != 'X' || buffer.get() != 'N' || buffer.get() != 'B') {
            throw new Exception("Not an XNB file: " + inputFile.getName());
        }

        int targetPlatform = buffer.get();
        int version = buffer.get();
        if (version != 5) {
            throw new Exception("Unsupported XNB version: " + version);
        }

        boolean compressed = (buffer.get() & 0x80) != 0;
        int compressedSize = buffer.getInt();
        int decompressedSize = compressed ? buffer.getInt() : compressedSize;

        if (compressed) {
            ByteBuffer decompressedBuffer = ByteBuffer.allocate(decompressedSize);
            decompressedBuffer.order(ByteOrder.LITTLE_ENDIAN);

            lzxDecoder.decompress(buffer, compressedSize - HEADER_SIZE, decompressedBuffer, decompressedSize);
            decompressedBuffer.position(0);
            buffer = decompressedBuffer;
        }

        int typeReaderCount = get7BitEncodedInt(buffer);
        String typeReaderName = getCSharpString(buffer);
        buffer.getInt(); // version

        int assemblyIdx = typeReaderName.indexOf(',');
        if (assemblyIdx != -1)
            typeReaderName = typeReaderName.substring(0, assemblyIdx);

        // Skip remaining type readers
        for (int k = 1; k < typeReaderCount; k++) {
            getCSharpString(buffer);
            buffer.getInt();
        }

        // Shared resources
        get7BitEncodedInt(buffer);
        // Primary asset flag
        get7BitEncodedInt(buffer);

        String xnbFileName = inputFile.getName();
        String baseFileName = xnbFileName.substring(0, xnbFileName.lastIndexOf('.'));

        // Determine output subdirectory
        String subDir = "Misc";
        if (baseFileName.startsWith("Item_")) subDir = "Items";
        else if (baseFileName.startsWith("NPC_")) subDir = "NPCs";
        else if (baseFileName.startsWith("Tiles_")) subDir = "Tiles";
        else if (baseFileName.startsWith("Wall_")) subDir = "Walls";
        else if (baseFileName.startsWith("Gore_")) subDir = "Gore";
        else if (baseFileName.startsWith("Projectile_")) subDir = "Projectiles";
        else if (baseFileName.startsWith("Player_")) subDir = "Player";
        else if (baseFileName.startsWith("Background_")) subDir = "Backgrounds";

        switch (typeReaderName) {
            case "Microsoft.Xna.Framework.Content.Texture2DReader": {
                int surfaceFormat = buffer.getInt();
                int width = buffer.getInt();
                int height = buffer.getInt();
                int mipCount = buffer.getInt();
                int size = buffer.getInt();

                if (surfaceFormat != 0) {
                    throw new Exception("Unsupported surface format: " + surfaceFormat + " in " + xnbFileName);
                }

                // Read RGBA pixel data
                BufferedImage image = new BufferedImage(width, height, BufferedImage.TYPE_INT_ARGB);
                for (int y = 0; y < height; y++) {
                    for (int x = 0; x < width; x++) {
                        int r = buffer.get() & 0xFF;
                        int g = buffer.get() & 0xFF;
                        int b = buffer.get() & 0xFF;
                        int a = buffer.get() & 0xFF;
                        int argb = (a << 24) | (r << 16) | (g << 8) | b;
                        image.setRGB(x, y, argb);
                    }
                }

                File outFile = new File(new File(outputDir, subDir), baseFileName + ".png");
                ImageIO.write(image, "png", outFile);
                System.out.println("Extracted: " + subDir + "/" + baseFileName + ".png (" + width + "x" + height + ")");
                break;
            }
            case "Microsoft.Xna.Framework.Content.SoundEffectReader": {
                int audioFormat = buffer.getInt();
                int wavCodec = buffer.getShort() & 0xFFFF;
                int channels = buffer.getShort() & 0xFFFF;
                int samplesPerSecond = buffer.getInt();
                int averageBytesPerSecond = buffer.getInt();
                int blockAlign = buffer.getShort() & 0xFFFF;
                int bitsPerSample = buffer.getShort() & 0xFFFF;
                buffer.getShort(); // Unknown
                int dataChunkSize = buffer.getInt();

                // Write WAV file
                File outFile = new File(new File(outputDir, "Sounds"), baseFileName + ".wav");
                try (FileOutputStream fos = new FileOutputStream(outFile)) {
                    ByteBuffer wav = ByteBuffer.allocate(44);
                    wav.order(ByteOrder.LITTLE_ENDIAN);
                    wav.put("RIFF".getBytes());
                    wav.putInt(dataChunkSize + 36);
                    wav.put("WAVE".getBytes());
                    wav.put("fmt ".getBytes());
                    wav.putInt(16);
                    wav.putShort((short) 1);
                    wav.putShort((short) channels);
                    wav.putInt(samplesPerSecond);
                    wav.putInt(averageBytesPerSecond);
                    wav.putShort((short) blockAlign);
                    wav.putShort((short) bitsPerSample);
                    wav.put("data".getBytes());
                    wav.putInt(dataChunkSize);
                    fos.write(wav.array());

                    byte[] audioData = new byte[dataChunkSize];
                    buffer.get(audioData);
                    fos.write(audioData);
                }
                System.out.println("Extracted: Sounds/" + baseFileName + ".wav");
                break;
            }
            case "Microsoft.Xna.Framework.Content.SpriteFontReader":
            case "Microsoft.Xna.Framework.Content.EffectReader": {
                throw new Exception("skipping " + typeReaderName.substring(typeReaderName.lastIndexOf('.') + 1));
            }
            default: {
                throw new Exception("Unsupported asset type: " + typeReaderName + " in " + xnbFileName);
            }
        }
    }

    static int get7BitEncodedInt(ByteBuffer buffer) {
        int result = 0;
        int bitsRead = 0;
        int value;
        do {
            value = buffer.get() & 0xFF;
            result |= (value & 0x7f) << bitsRead;
            bitsRead += 7;
        } while ((value & 0x80) != 0);
        return result;
    }

    static String getCSharpString(ByteBuffer buffer) {
        int len = get7BitEncodedInt(buffer);
        byte[] buf = new byte[len];
        buffer.get(buf);
        return new String(buf, Charset.forName("UTF-8"));
    }
}
