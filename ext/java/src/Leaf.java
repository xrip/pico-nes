import java.util.List;
import java.util.Set;

class Leaf implements Node {
    List<Block> blocks;
    public Leaf(List<Block> blocks) {
        this.blocks = blocks;
    }
    @Override
    public int power() {
        return blocks.size() * 4 + 4;
    }
    @Override
    public int size() {
        return 1;
    }
    @Override
    public int power(Set<Node> calculated) {
        return blocks.size() * 4;
    }
    @Override
    public int hashCode() {
        return blocks.hashCode();
    }
    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null) return false;
        if (this.getClass() != o.getClass()) return false;
        final Leaf l = (Leaf) o;
        return l.blocks.equals(this.blocks);
    }
    @Override
    public String toString() {
        return blocks.toString();
    }
}
