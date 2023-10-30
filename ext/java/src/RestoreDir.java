import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Paths;

public class RestoreDir {
    public static void main(final String[] args) {
        try {
            final byte[] data = Files.readAllBytes(Paths.get(args[0]));
            int offset = 0;
            int dirSize = convertToInt4(data, offset);
            offset += 4;
            while (offset < dirSize) {
                final StringBuilder s = new StringBuilder();
                for (; data[offset] != 0; ++offset) {
                    byte k8r = data[offset];
                    char c = (char) (k8r & 0xFF);
                    s.append(c);
                }
                ++offset;
                int fileSize = convertToInt4(data, offset);
                offset += 4;
                int fileOffset = convertToInt4(data, offset);
                offset += 4;
                System.out.println("p: " + s + "; fileSize: " + fileSize + "; fileOffset: " + fileOffset);
            }
            // TODO:

            int size = convertToInt4(data, offset);
            offset += 4;
            final byte[] res = new byte[size];
            int references = convertToInt3(data, offset);
            offset += 3;
            System.out.println("sz: " + size + "; refs: " + references);
            int blocksDescOffset = offset;
            int dataOffset = blocksDescOffset + references * 3;
            int targetIdx = 0;
            while (blocksDescOffset < dataOffset) {
                int chipper = convertToInt3(data, blocksDescOffset);
                int plusOffset = chipper & 0xFFFFF;
                int szCode = (chipper & 0xF00000) >> 20;
                int sz = 1 << (szCode + 2);
                plusOffset <<= 2; // blocks to bytes
                final var off = dataOffset + plusOffset;
                System.out.println("szCode: " + szCode + "; sz:" + sz + " plusOffset: " + plusOffset + " + dataOffset: " + dataOffset + " = " + off);
                for (int i = off; i < off + sz; ++i) {
                    res[targetIdx++] = data[i];
                }
                blocksDescOffset += 3;
            }
            try (var outputStream = Files.newOutputStream(Paths.get("original.nes"))) {
                outputStream.write(res);
            }
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }
    private static int convertToInt3(byte[] data, int i) {
        return  (data[i]<<16) & 0x00ff0000 |
                (data[i+1]<<8) & 0x0000ff00 |
                (data[i+2]) & 0x000000ff;
    }
    private static int convertToInt4(byte[] data, int i) {
        return (data[i]<<24) & 0xff000000 |
                (data[i+1]<<16) & 0x00ff0000 |
                (data[i+2]<< 8) & 0x0000ff00 |
                (data[i+3]) & 0x000000ff;
    }
}
