package net.jpountz.lz4;

import java.io.File;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.*;

public class Analyze {
    public static void main(final String[] args) {
        try {
            final File folder = new File(args[0]);
            final File[] listOfFiles = folder.listFiles();
            assert listOfFiles != null;
            final Map<List<Byte>, Integer> stat = new HashMap<>();
            for (final File file : listOfFiles) {
                if (file.isFile()) {
                    System.out.println(file.getName());
                    final byte[] data = Files.readAllBytes(Paths.get(args[0] + "\\" + file.getName()));
                    for (int i = 0; i < data.length; i += 2) {
                        final List<Byte> _4b = new ArrayList<>();
                        _4b.add(data[i]);
                        _4b.add(data[i+1]);
//                        _4b.add(data[i+2]);
//                        _4b.add(data[i+3]);
                        stat.merge(_4b, 1, Integer::sum);
                    }
                }
            }
            System.out.println("sz: " + stat.size());
            for (final var e: stat.entrySet()) {
                if (e.getValue() > 1) System.out.println(e.getKey() + ": " + e.getValue());
            }
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }
}
