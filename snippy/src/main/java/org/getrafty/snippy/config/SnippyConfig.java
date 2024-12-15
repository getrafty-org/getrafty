package org.getrafty.snippy.config;

import com.intellij.openapi.project.Project;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Objects;
import java.util.Properties;

public class SnippyConfig {
    private static final String CONFIG_FILE_NAME = ".snippy";
    private static final String DEFAULT_SNIPPET_STORAGE = ".snippets";
    private final Properties properties;

    public SnippyConfig(Project project) {
        Path configFile = Path.of(Objects.requireNonNull(project.getBasePath()), CONFIG_FILE_NAME);
        properties = new Properties();
        if (Files.exists(configFile)) {
            try {
                properties.load(Files.newBufferedReader(configFile));
            } catch (IOException e) {
                throw new RuntimeException("Failed to load .snippy configuration", e);
            }
        }
    }

    public String getSnippetStoragePath() {
        return properties.getProperty("snippetStoragePath", DEFAULT_SNIPPET_STORAGE);
    }
}
