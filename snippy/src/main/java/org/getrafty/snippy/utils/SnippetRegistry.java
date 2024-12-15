package org.getrafty.snippy.utils;

import java.util.HashSet;
import java.util.Set;

public class SnippetRegistry {
    private static final Set<String> registeredSnippets = new HashSet<>();

    public static synchronized boolean isSnippetUnique(String snippetId) {
        return !registeredSnippets.contains(snippetId);
    }

    public static synchronized void registerSnippet(String snippetId) {
        registeredSnippets.add(snippetId);
    }
}
