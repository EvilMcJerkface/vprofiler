import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Scanner;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

class CodeChanger {
	String code = "";
	Scanner in;
	String targetf;
	PrintWriter out;
	String buffer = "";
	String includes = "", beforeDef, def, func, afterDef, rType;
	ArrayList<String> funcs = new ArrayList<String>();
	int sc = -1;
	int funcCnt = 0;
	Boolean resultVal = false, switchVal = false, bTrace = false,
			dontChange = false;

	public static void main(String[] args) {
		if (args.length < 2) {
			System.err.println("This program takes 2 arguments.");
			return;
		}
		try {
			Scanner in = new Scanner(new FileReader(args[0]));
			CodeChanger cc = new CodeChanger(in, args[1]);
			String res = cc.run();
			cc.printOutput();
//			System.out.println(res);
			String[] path = args[0].split("/");
			FileWriter outFile = new FileWriter(path[path.length - 1]);
			PrintWriter out = new PrintWriter(outFile);
			out.print(res);
			out.close();
		} catch (FileNotFoundException e) {
			System.err.println("File not found!");
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}

	CodeChanger(Scanner in, String targetf) {
		this.in = in;
		// this.out = out;
		this.targetf = targetf;
		while (in.hasNext()) {
			code += in.nextLine() + "\n";
		}
	}

	public void printOutput() {
		System.out.println(funcCnt);
		HashMap<String, Integer> fs = new HashMap<String, Integer>();
		for (String s : funcs) {
			if (fs.containsKey(s)) {
				if (fs.get(s).equals(0))
					fs.put(s, 2);
				else
					fs.put(s, fs.get(s) + 1);
			} else
				fs.put(s, 0);
		}
		String res = "[";
		for (String s : funcs) {
			if (fs.get(s).equals(0)) {
				res += "'" + s + "', ";
			} else {
				res += "'" + s + "-" + fs.get(s) + "', ";
				fs.put(s, fs.get(s) - 1);
			}
		}
		res = res.substring(0, res.length() - 2) + "]";
		System.out.println(res);
	}

	String run() {
		code = removeComments(code);
		findSegments();
		String res = includes, output = "";
		res += "\n#include \"vprofiler/trace_tool.h\"\n";
		String s;
		while ((s = nextStatement()) != null) {
			output += fix(s);
		}
		if (resultVal) {
			output = "\t" + rType + " resultReturn;\n" + output;
		}
		if (switchVal) {
			output = "\tint resultSwitch;\n" + output;
		}
		// if (rType == "void")
		output += "\n\n\tTRACE_FUNCTION_END();\n";
		if (bTrace) {
			// res += "\n\n#define TRACE_S_E(x,n) (BOOLEAN_TRACE_START()|(x)|BOOLEAN_TRACE_END(n))";
			// res += "\nbool BOOLEAN_TRACE_START(){\n\tTRACE_START(); return false;\n}\nbool BOOLEAN_TRACE_END(int n){\n\tTRACE_END(n); return false;\n}\n\n";
		}
		res += beforeDef;
		res += def;
		res += "\n\tTRACE_FUNCTION_START();\n";
		res += output;
		res += afterDef;
		return res;
	}

	private String fix(String s) {
		if (dontChange) {
			return s;
		}
		if (isReturn(s)) {
			return fixReturn(s);
		}
		if (isSwitch(s)) {
			return fixSwitch(s);
		}
		if (isFor(s)) {
			return fixFor(s);
		}
		if (isCondition(s)) {
			return fixConditionStatement(s);
		}
		if (isElse(s)) {
			return fixElse(s);
		}
		return fixStatement(s);
	}

	private String fixSwitch(String s) {
		String[] parts = getConditionParts(s);
		String val = parts[1];
		if (!hasFunction(val))
			return s;
		Pattern p = Pattern.compile("^(\\s*)switch");
		Matcher m = p.matcher(parts[0]);
		m.find();
		String sp = m.group(1), tabs = getTabs(sp), as = parts[2];
		switchVal = true;
		String f = getFunctionName(val);
		addFunction(f);
		return sp + "TRACE_START();" + tabs + "resultSwitch = " + val + ";"
				+ tabs + "TRACE_END(" + funcCnt + ");\n" + tabs
				+ "switch (resultSwitch" + as;
	}

	private boolean isSwitch(String s) {
		Pattern p = Pattern.compile("^(\\s*)switch\\s*\\(");
		Matcher m = p.matcher(s);
		if (m.find())
			return true;
		return false;
	}

	private String fixElse(String s) {
		String output = s;
		if (s.endsWith("{")) {
			int pn = 0;
			String ss;
			while ((ss = nextStatement()) != null) {
				output += fix(ss);
				if (ss.endsWith("{") && !isCondition(ss) && !isFor(ss)
						&& !isElse(ss))
					pn++;
				if (ss.endsWith("}")) {
					pn--;
					if (pn == -1)
						break;
				}
			}
		} else {
			String ns = nextStatement();
			if (hasFunction(ns) || isReturn(ns) || isCondition(ns)
					|| isElse(ns) || isFor(ns)) {
				output += " {\n";
				output += fix(ns) + "\n} ";
			} else
				output += ns;
		}
		return output;
	}

	private String fixConditionStatement(String s) {
		String[] parts = getConditionParts(s);
		String output;
		if (s.endsWith("{")) {
			output = parts[0] + fixCondition(parts[1]) + parts[2];
			int pn = 0;
			String ss;
			while ((ss = nextStatement()) != null) {
				output += fix(ss);
				if (ss.endsWith("{") && !isCondition(ss) && !isFor(ss)
						&& !isElse(ss))
					pn++;
				if (ss.endsWith("}")) {
					pn--;
					if (pn == -1)
						break;
				}
			}
		} else {
			if (hasFunction(parts[1]))
				output = parts[0] + fixCondition(parts[1]);
			else
				output = parts[0] + parts[1];
			String ns = nextStatement();
			if (hasFunction(ns) || isReturn(ns) || isCondition(ns)
					|| isElse(ns) || isFor(ns)) {
				parts[2] += " {";
				output += parts[2] + fix(ns) + "\n} ";
			} else
				output += parts[2] + ns;
		}
		return output;
	}

	private String fixStatement(String s) {
		if (!hasFunction(s))
			return s;
		Pattern fh = Pattern.compile("^(\\s*)");
		Matcher matcher = fh.matcher(s);
		matcher.find();
		String f = getFunctionName(s);
		addFunction(f);
		String tabs = getTabs(matcher.group(1));
		return matcher.group(1) + "TRACE_START();" + tabs
				+ s.substring(matcher.end()) + tabs + "TRACE_END(" + funcCnt
				+ ");";
	}

	private String fixFor(String s) {
		String[] parts = getConditionParts(s);
		Pattern p = Pattern.compile("^(\\s*)for\\s*\\(");
		Matcher m = p.matcher(parts[0]);
		m.find();
		String b = m.group(1), in = parts[0].substring(m.end()), tabs = getTabs(b);
		p = Pattern.compile("\\)\\s*(\\{)?$");
		m = p.matcher(parts[2]);
		m.find();
		String e = m.group(), st = parts[2].substring(1, m.start()), stO = "", output = "";
		boolean inHas = false, stHas = false;
		output += b;
		if (hasFunction(in)) {
			inHas = true;
			output += "{\n";
			tabs += "\t";
			output += fixStatement(tabs + in) + tabs;
		}
		output += "for (";
		if (!inHas)
			output += in;
		else
			output += ";";
		String cond = hasFunction(parts[1]) ? fixCondition(parts[1]) : parts[1];
		output += cond + " ;";
		if (hasFunction(st)) {
			stHas = true;
			stO = fixStatement(tabs + st + ';');
			output += " ";
		} else
			output += st;
		if (s.endsWith("{")) {
			output += e;
			int pn = 0;
			String ss;
			while ((ss = nextStatement()) != null) {
				if (ss.endsWith("{") && !isCondition(ss) && !isFor(ss)
						&& !isElse(ss))
					pn++;
				if (ss.endsWith("}")) {
					pn--;
					if (pn == -1)
						break;
				}
				output += fix(ss);
			}
			if (stHas)
				output += "\n" + stO;
			output += tabs + "}";
		} else {
			output += " ) {";
			output += fix(nextStatement());
			if (stHas)
				output += "\n" + stO;
			output += tabs + "}";
		}
		if (inHas)
			output += "\n" + tabs.substring(0, tabs.length() - 1) + "}";
		// System.out.println(b);
		// System.out.println(in);
		// System.out.println(parts[1]);
		// System.out.println(st);
		// System.out.println(e);
		return output;
	}

	private String fixReturn(String s) {
		Pattern p = Pattern
				.compile("^(\\s*)return(;|\\s+([^;]*);|(\\W+[^;]*);)(\\s*)$");
		Matcher m = p.matcher(s);
		if (m.find()) {
			String sp = m.group(1);
			String val = m.group(3);
			if (val == null)
				val = m.group(4);
			String sp2 = m.group(5);
			String tabs = getTabs(sp);
			if (val != null && hasFunction(val)) {
				resultVal = true;
				String f = getFunctionName(val);
				addFunction(f);
				return sp + "TRACE_START();" + tabs + "resultReturn = " + val
						+ ";" + tabs + "TRACE_END(" + funcCnt + ");\n" + tabs
						+ "TRACE_FUNCTION_END();" + tabs
						+ "return resultReturn;" + sp2;
			}
			return sp + "TRACE_FUNCTION_END();" + tabs + "return "
					+ (val == null ? "" : val) + ";" + sp2;
		}
		p = Pattern.compile("^(\\s*)DBUG_RETURN\\s*\\(\\s*(.*)\\)\\s*;(\\s*)");
		m = p.matcher(s);
		if (m.find()) {
			String sp = m.group(1);
			String val = m.group(2);
			String sp2 = m.group(3);
			String tabs = getTabs(sp);
			if (hasFunction(val)) {
				resultVal = true;
				String f = getFunctionName(val);
				addFunction(f);
				return sp + "TRACE_START();" + tabs + "resultReturn = " + val
						+ ";" + tabs + "TRACE_END(" + funcCnt + ");\n" + tabs
						+ "TRACE_FUNCTION_END();" + tabs
						+ "DBUG_RETURN(resultReturn);" + sp2;
			}
			return sp + "TRACE_FUNCTION_END();" + tabs + "DBUG_RETURN(" + val
					+ ");" + sp2;
		}
		p = Pattern.compile("^(\\s*)(DBUG_VOID_RETURN\\s*\\(.*)$");
		m = p.matcher(s);
		if (m.find()) {
			String sp = m.group(1);
			String sp2 = m.group(2);
			String tabs = getTabs(sp);
			return sp + "TRACE_FUNCTION_END();" + tabs + sp2;
		}
		return s;
	}

	private String getTabs(String sp) {
		Pattern p2 = Pattern.compile("(\\s*)\n(\\s*?)$");
		Matcher matcher2 = p2.matcher(sp);
		String tabs = "\n";
		if (matcher2.find()) {
			tabs += matcher2.group(2);
		}
		return tabs;
	}

	private boolean isReturn(String s) {
		Pattern p = Pattern
				.compile("^(\\s*)return(;|\\s+([^;]*);|(\\W+[^;]*);)\\s*$");
		Matcher m = p.matcher(s);
		if (m.find())
			return true;
		p = Pattern.compile("^(\\s*)(DBUG_RETURN|DBUG_VOID_RETURN)\\s*\\(");
		m = p.matcher(s);
		if (m.find())
			return true;
		return false;
	}

	private String fixCondition(String s) {
		if (!hasFunction(s))
			return s;

		if (s.contains("||") || s.contains("&&")) {
			int i, p = 0;
			boolean instr = false;
			char strop = ' ';
			for (i = s.length() - 1; i >= 0; i--) {
				char c = s.charAt(i);
				if ((!instr && (c == '\"' || c == '\''))
						&& (i < 1 || s.charAt(i - 1) != '\\')) {
					instr = true;
					strop = c;
					continue;
				} else if ((instr && (c == strop))
						&& (i < 1 || s.charAt(i - 1) != '\\')) {
					instr = false;
					continue;
				} else if (instr)
					continue;
				else if (c == '(')
					p++;
				else if (c == ')')
					p--;
				else if (p != 0)
					continue;
				else if (c == '|' || c == '&') {
					String op, s1, s2;
					if (i > 0 && s.charAt(i - 1) == c) {
						op = c + "" + c;
						s1 = s.substring(0, i - 1);
						s2 = s.substring(i + 1);
					} else {
						op = c + "";
						s1 = s.substring(0, i);
						s2 = s.substring(i + 1);
					}
					s1 = fixCondition(s1);
					s2 = fixCondition(s2);
					return s1 + " " + op + " " + s2;
				}
			}
			Pattern fh = Pattern.compile("^(\\s*\\()(.*)(\\)\\s*)$");
			Matcher matcher = fh.matcher(s);
			if (matcher.find()) {
				String l = matcher.group(1), c = fixCondition(matcher.group(2)), r = matcher
						.group(3);// , rest=l+" "+r;
				// if(hasFunction(rest)){
				// String f = getFunctionName(rest);
				// addFunction(f);
				// return "TRACE_S_E( " + l + c + r + " ," + funcCnt + ")";
				// }
				return l + c + r;
			}
		}
		String f = getFunctionName(s);
		addFunction(f);
		bTrace = true;
		return "TRACE_S_E( " + s + " ," + funcCnt + ")";
	}

	private void addFunction(String f) {
		funcCnt++;
		funcs.add(f);
	}

	private String getFunctionName(String s) {
		s = s.replaceAll("\\((\\w)", "( $1");
		Pattern fh = Pattern
				.compile("(^|\\W+)(?!for|if|while|else|catch)(\\w+)\\s*\\(");
		Matcher matcher = fh.matcher(s);
		String res = "";
		if (matcher.find())
			res = matcher.group(2);
		if (matcher.find())
			res = matcher.group(2);
		return res;
	}

	private String[] getConditionParts(String s) {
		Pattern fh = Pattern.compile("(^|\\W+)(for|if|while|switch)\\s*\\(");
		Matcher matcher = fh.matcher(s);
		matcher.find();
		int b = matcher.end(), p = 0, i = b - 1;
		boolean instr = false;
		char strop = ' ';
		while (++i < s.length()) {
			char c = s.charAt(i);
			if ((!instr && (c == '\"' || c == '\''))
					&& (i < 1 || s.charAt(i - 1) != '\\')) {
				instr = true;
				strop = c;
				continue;
			} else if ((instr && (c == strop))
					&& (i < 1 || s.charAt(i - 1) != '\\')) {
				instr = false;
				continue;
			} else if (instr)
				continue;
			else if (c == '(')
				p++;
			else if (c == ')') {
				p--;
				if (p == -1) {
					break;
				}
			}
		}
		if (matcher.group(2).equals("for")) {
			while (s.charAt(b++) != ';')
				;
			while (s.charAt(--i) != ';')
				;
		}
		return new String[] { s.substring(0, b), s.substring(b, i),
				s.substring(i) };
	}

	private boolean isFor(String s) {
		Pattern fh = Pattern.compile("(^|\\W+)for\\s*\\(");
		Matcher matcher = fh.matcher(s);
		return matcher.find();
	}

	private boolean isCondition(String s) {
		Pattern fh = Pattern.compile("(^|[^\\w#]+)(if|while)\\s*(\\()");
		Matcher matcher = fh.matcher(s);
		return matcher.find();
	}

	private boolean isElse(String s) {
		Pattern fh = Pattern.compile("(^|\\W+)else(\\s+(\\{)?|\\{)");
		Matcher matcher = fh.matcher(s);
		return matcher.find();
	}

	private boolean hasFunction(String s) {
		Pattern fh = Pattern
				.compile("(^|\\W+)(?!for|if|while|else|ut_ad|ereport|UNIV_UNLIKELY|UNIV_EXPECT|UNIV_MEM_INVALID|UNIV_LIKELY|catch|TRX_START|TRX_END|PATH_INC|PATH_DEC|COMMIT)(\\w+)\\s*\\(");
		Matcher matcher = fh.matcher(s);
		return matcher.find();
	}

	private String nextStatement() {
		dontChange = false;
		int l = sc + 1, p = 0, e = 0;
		boolean onPC = false;
		String funcc;
		Pattern p2;
		Matcher m2;
		if (sc + 1 < func.length()) {
			funcc = func.substring(sc + 1);
			p2 = Pattern.compile("^\\s*((else\\s+)?if|while|for)\\s*\\(");
			m2 = p2.matcher(funcc);
			if (m2.find()) {
				sc += m2.end();
				p = 1;
				onPC = true;
			} else {
				p2 = Pattern.compile("^\\s*else(\\s+(\\{)?|\\{)");
				m2 = p2.matcher(funcc);
				if (m2.find()) {
					sc += m2.end();
					return m2.group();
				} else {
					p2 = Pattern.compile("^(\\s*)#[^\n]*");
					m2 = p2.matcher(funcc);
					if (m2.find()) {
						sc += m2.end();
						dontChange = true;
						return m2.group();
					}
				}
			}
		}
		boolean instr = false;
		char strop = ' ';
		while (++sc < func.length()) {
			char c = func.charAt(sc);
			if ((!instr && (c == '\"' || c == '\''))
					&& (sc < 1 || func.charAt(sc - 1) != '\\')) {
				instr = true;
				strop = c;
				continue;
			} else if ((instr && (c == strop))
					&& (sc < 1 || func.charAt(sc - 1) != '\\')) {
				instr = false;
				continue;
			} else if (instr)
				continue;
			else if (c == '(')
				p++;
			else if (c == ')') {
				p--;
				if (onPC && p == 0) {
					p2 = Pattern.compile("^\\s*\\{");
					m2 = p2.matcher(func.substring(sc + 1));
					if (m2.find()) {
						sc += m2.end();
					}
					e = sc + 1;
					break;
				}
			} else if (p != 0)
				continue;
			else if (c == ';' || c == '{' || c == '}' || c == ':') {
				e = sc + 1;
				break;
			} else if (c == '/'
					&& (sc < func.length() - 1 && (func.charAt(sc + 1) == '*' || func
							.charAt(sc + 1) == '/'))) {
				e = sc;
				sc--;
				break;
			}
		}
		if (sc == l && e == 0)
			return null;
		if (e != 0)
			return func.substring(l, e);
		return func.substring(l);
	}

	private void findSegments() {
        System.out.println(targetf);
		Pattern fh = Pattern.compile("(\\w+(\\s|(\\s*\\*)+))\\s*"
				+ this.targetf + "\\s*\\([^\\)]*\\)\\s*\\{\\s*");
		Matcher matcher = fh.matcher(code);
		if (!matcher.find()) {
			System.err.println("The function does not exist.");
			throw new RuntimeException("The function does not exist.");
		}
		beforeDef = code.substring(0, matcher.start());
		int ifi = beforeDef.indexOf("#if");
		String bda = "";
		if (ifi != -1) {
			bda = beforeDef.substring(ifi);
			beforeDef = beforeDef.substring(0, ifi);
		}
		Pattern p2 = Pattern.compile("\\s*#include\\s+.*\n");
		Matcher m2 = p2.matcher(beforeDef);
		int ii = 0;
		while (m2.find()) {
			ii = m2.end();
		}
		includes = beforeDef.substring(0, ii);
		beforeDef = beforeDef.substring(ii) + bda;

		def = matcher.group();
		int i = matcher.end() - 1, p = 0;
		boolean instr = false;
		char strop = ' ';
		while (++i < code.length()) {
			char c = code.charAt(i);
			if ((!instr && (c == '\"' || c == '\''))
					&& (i < 1 || code.charAt(i - 1) != '\\')) {
				instr = true;
				strop = c;
				continue;
			} else if ((instr && (c == strop))
					&& (i < 1 || code.charAt(i - 1) != '\\')) {
				instr = false;
				continue;
			} else if (instr)
				continue;
			else if (c == '{')
				p++;
			else if (c == '}') {
				p--;
				if (p == -1)
					break;
			}
		}
		func = code.substring(matcher.end(), i);
		afterDef = code.substring(i);
		rType = matcher.group(1);
	}

	String removeComments(String code) {
		code = code.replaceAll("/\\*([^\\*]*\\*[^/])*?[^\\*]*\\*+/", "");
		return code.replaceAll("(?<!:)//[^\n]*\n", "\n");
	}
}
