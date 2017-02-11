public class Utils {
    public static boolean AnyNull(Object ... objects) {
        for (Object obj : objects) {
            if (obj == null) {
                return true;
            }
        }

        return false;
    }
}
