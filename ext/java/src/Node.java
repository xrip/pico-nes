import java.util.Set;

public interface Node {
    int power();
    int size();
    int power(Set<Node> calculated);
}
