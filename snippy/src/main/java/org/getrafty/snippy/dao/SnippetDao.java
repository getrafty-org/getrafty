package org.getrafty.snippy.dao;

import com.intellij.openapi.project.Project;
import org.getrafty.snippy.config.SnippyConfig;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;

public class SnippetDao {
    private final Path snippetStoragePath;

    public SnippetDao(Project project) {
        SnippyConfig config = new SnippyConfig(project);
        this.snippetStoragePath = Path.of(project.getBasePath(), config.getSnippetStoragePath());
        try {
            Files.createDirectories(snippetStoragePath);
        } catch (IOException e) {
            throw new RuntimeException("Failed to create snippet storage directory", e);
        }
    }

    public void saveSnippet(String hash, String content, String mode) {
        Path snippetFile = snippetStoragePath.resolve(hash + "." + mode + ".snippet");
        try {
            Files.writeString(snippetFile, content, StandardOpenOption.CREATE, StandardOpenOption.TRUNCATE_EXISTING);
        } catch (IOException e) {
            throw new RuntimeException("Failed to save snippet", e);
        }
    }

    public String loadSnippet(String hash, String mode) {
        Path snippetFile = snippetStoragePath.resolve(hash + "." + mode + ".snippet");
        try {
            if (Files.exists(snippetFile)) {
                return Files.readString(snippetFile);
            }
        } catch (IOException e) {
            throw new RuntimeException("Failed to load snippet", e);
        }
        return "";
    }
}
