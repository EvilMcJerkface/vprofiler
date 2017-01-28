public class Utils {
    public static boolean AnyNull(Object ... objects) {
        for (Object obj : objects) {
            if (obj == null) {
                return false;
            }
        }

        return true;
    }
}
