package org.getrafty.snippy.listeners;

import com.intellij.openapi.editor.Document;
import com.intellij.openapi.fileEditor.FileDocumentManagerListener;
import com.intellij.openapi.project.Project;
import org.getrafty.snippy.services.SnippetService;
import org.jetbrains.annotations.NotNull;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class SnippetFileSaveListener implements FileDocumentManagerListener {
    private final Project project;

    public SnippetFileSaveListener(Project project) {
        this.project = project;
    }

    @Override
    public void beforeDocumentSaving(@NotNull Document document) {
        // Get the SnippetService instance for the project
        SnippetService snippetService = project.getService(SnippetService.class);

        // Get the text of the current document
        String text = document.getText();

        // Regex to find snippets marked with the unique hash
        var pattern = Pattern.compile("// ==== YOUR CODE: @(.*?) ====(.*?)// ==== END YOUR CODE ====", Pattern.DOTALL);
        var matcher = pattern.matcher(text);

        while (matcher.find()) {
            var hash = matcher.group(1).trim();
            var snippetContent = matcher.group(2);

            // Save the snippet content using SnippetService
            snippetService.saveSnippet(hash, snippetContent);
        }
    }
}
