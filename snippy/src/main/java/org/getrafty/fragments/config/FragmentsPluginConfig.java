package org.getrafty.fragments.config;

import com.intellij.openapi.project.Project;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Objects;
import java.util.Properties;

public class FragmentsPluginConfig {
    private static final String CONFIG_FILE_NAME = ".fragments.cfg";
    private static final String DEFAULT_FRAGMENTS_FOLDER = ".fragments";
    public static final String PROP_FRAGMENTS_STORAGE_FOLDER = "fragments.folder";

    private final Properties properties = new Properties();

    public FragmentsPluginConfig(Project project) {
        var configFile = Path.of(Objects.requireNonNull(project.getBasePath()), CONFIG_FILE_NAME);

        if (!Files.exists(configFile)) {
            return;
        }

        try {
            properties.load(Files.newBufferedReader(configFile));
        } catch (IOException e) {
            throw new RuntimeException("Failed to load Fragments configuration", e);
        }
    }

    public String getSnippetStoragePath() {
        return properties.getProperty(PROP_FRAGMENTS_STORAGE_FOLDER, DEFAULT_FRAGMENTS_FOLDER);
    }
}
