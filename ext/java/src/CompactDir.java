import java.io.File;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.*;

public class CompactDir {
    public static void main(final String[] args) {
        try {
            final File folder = new File(args[0]);
            final File[] listOfFiles = folder.listFiles();
            assert listOfFiles != null;

            Branch tree = new Branch();
            int dataSize = 0;

            final List<String> files = new ArrayList<>();
            final Map<String, Integer> fileToSize = new TreeMap<>();
            for (File file : listOfFiles) {
                if (file.isFile()) {
                    System.out.println(file.getName());
                    final byte[] data = Files.readAllBytes(Paths.get(args[0] + "\\" + file.getName()));
                    dataSize += data.length;
                    fileToSize.put(file.getName(), data.length);
                    files.add(file.getName());
                    final var branch = new Branch();
                    final var leaf = new Leaf(new ArrayList<>());
                    branch.nodes.add(leaf);
                    tree.nodes.add(branch);
                    for (int i = 0; i < data.length; i += 4) {
                        leaf.blocks.add(new Block(Main.convertToInt(data, i, data.length - i)));
                    }
                }
            }

            System.out.println("power: " + tree.power() + "; size: " + tree.size());
            int size = 1024;
            while (size > 1) {
                tree = Main.compress(tree, size);
                tree = Main.compress(tree, size);
                size /= 2;
            }
            tree = Main.compress(tree, 1);
            tree = Main.compress(tree, 1);
            System.out.println("power: " + tree.power() + "; size: " + tree.size());

            final Map<List<Block>, Integer> nodes2offset = new HashMap<>();
            final List<List<Block>> blocks = new ArrayList<>();
            for (final var n: tree.nodes) {
                if (n instanceof Branch b) { // TODO: recursive
                    for (final var n1: b.nodes) {
                        final var k = ((Leaf)n1).blocks;
                        var offset = nodes2offset.get(k);
                        if (offset == null) {
                            offset = Main.calcOffset(blocks);
                            nodes2offset.put(k, offset);
                            blocks.add(k);
                        }
//                        System.out.println("np: " + n1.power() + "; offset: " + offset);
                    }
                }
            }

            List<Command> commands = new ArrayList<>();
            int fn = 0;
            final Map<String, Integer> name2offset = new TreeMap<>();
            Integer offsetFF = 0;
            for (final var n: tree.nodes) {
                if (n instanceof Branch b) { // TODO: recursive
                    final var fnS = files.get(fn++);
                    name2offset.put(fnS, offsetFF);
                    for (final var n1: b.nodes) {
                        final var k = ((Leaf)n1).blocks;
                        offsetFF = nodes2offset.get(k);
                        if (offsetFF == null) {
                            throw new RuntimeException("Unexpected offset");
                        }
                        int szCode = Main.extracted(k.size());
//                System.out.println("szCode: " + szCode + "; off: " + offset);
                        commands.add(new Command(szCode, offsetFF));
                    }
                }
            }
            System.out.println("compact: " + commands.size());
            int prevCommandsCount = -1;
            while (prevCommandsCount != commands.size()) {
                prevCommandsCount = commands.size();
                commands = Main.compactCommands(commands);
            }
            System.out.println("to: " + commands.size());

            final List<Byte> buff0 = new ArrayList<>();
            for (final var file : files) {
                for (final var b : file.getBytes("koi8-r")) {
                    buff0.add(b);
                }
                buff0.add((byte)0);
                buff0.addAll(Main.toBytes(fileToSize.get(file)));
                buff0.addAll(Main.toBytes(name2offset.get(file)));
            }

            // | dir size 4 bytes | file str0 | size 4b | offset 4b | ...
            // | src size 4 bytes | block-refs 3 bytes | sz block0 size code 4 bit | block1 offset (blocks) 20 bit | ...
            // 0 - 4b; 1 - 8b; 2 - 16b; 3 - 32b; 4 - 64b; 5 - 128b; 6 - 256b; 7 - 512b; 8 - 1024b; 9 - 2Kb..
            System.out.println("sz: " + dataSize + "; refs: " + commands.size() + "; blocks: " + blocks.size());
            final List<Byte> buff = new ArrayList<>();
            buff.addAll(Main.toBytes(buff0.size()));
            buff.addAll(buff0);
            buff.addAll(Main.toBytes(dataSize));
            buff.addAll(Main.toBytes3(commands.size()));
            for (final var c: commands) {
                System.out.println("szCode: " + c.szCode() + "; off: " + c.offset());
                buff.addAll(Main.toBytes3(c.szCode(), c.offset()));
            }
            for (final var b: blocks) {
                for (final var bi: b) {
                    buff.addAll(Main.toBytes(bi._4bytes()));
                }
            }

            byte[] arr = new byte[buff.size()];
            for (int i = 0; i < buff.size(); i++) {
                arr[i] = buff.get(i);
            }
            try (var outputStream = Files.newOutputStream(Paths.get("nes.pz"))) {
                outputStream.write(arr);
            }
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }
}
