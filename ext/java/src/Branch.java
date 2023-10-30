import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

public class Branch implements Node {
    List<Node> nodes = new ArrayList<>();
    @Override
    public int power() {
        final Set<Node> calculated = new HashSet<>();
        return power(calculated);
    }
    public int size() {
        int res = 0;
        for (var n : nodes) {
            res += n.size();
        }
        return res;
    }
    @Override
    public int power(Set<Node> calculated) {
        int res = 0;
        for (var n : nodes) {
            if (calculated.contains(n)) {
                res += 3;
                continue;
            }
            calculated.add(n);
            res += n.power(calculated);
        }
        return res;
    }
    @Override
    public int hashCode() {
        return nodes.hashCode();
    }
    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null) return false;
        if (this.getClass() != o.getClass()) return false;
        final Branch l = (Branch) o;
        return l.nodes.equals(this.nodes);
    }
}
