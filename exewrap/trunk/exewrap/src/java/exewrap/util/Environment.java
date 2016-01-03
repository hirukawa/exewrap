package exewrap.util;

import java.util.Collection;
import java.util.Map;
import java.util.Set;

import exewrap.core.ExewrapClassLoader;

public class Environment implements Map<String, String> {

	@Override
	public void clear() {
	}

	@Override
	public boolean containsKey(Object key) {
		return false;
	}

	@Override
	public boolean containsValue(Object value) {
		return false;
	}

	@Override
	public Set<java.util.Map.Entry<String, String>> entrySet() {
		return null;
	}

	@Override
	public String get(Object key) {
		return null;
	}

	@Override
	public boolean isEmpty() {
		return true;
	}

	@Override
	public Set<String> keySet() {
		return null;
	}

	@Override
	public String put(String key, String value) {
		return ExewrapClassLoader.SetEnvironment(key, value);
	}

	@Override
	public void putAll(Map<? extends String, ? extends String> m) {
		if(m == null) {
			return;
		}
		for(Map.Entry<? extends String, ? extends String> entry : m.entrySet()) {
			String key = entry.getKey();
			String value = entry.getValue();
			put(key, value);
		}
	}

	@Override
	public String remove(Object key) {
		return null;
	}

	@Override
	public int size() {
		return 0;
	}

	@Override
	public Collection<String> values() {
		return null;
	}
}
