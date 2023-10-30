import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Paths;

public class Depack {
    public static void main(final String[] args) {
        try {
            final byte[] data = Files.readAllBytes(Paths.get(args[0]));
            int size = convertToInt4(data, 0);
            final byte[] res = new byte[size];
            int references = convertToInt3(data, 4);
            System.out.println("sz: " + size + "; refs: " + references);
            int blocksDescOffset = 7;
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
