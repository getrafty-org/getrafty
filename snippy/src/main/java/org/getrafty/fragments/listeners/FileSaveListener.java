package org.getrafty.fragments.listeners;

import com.intellij.openapi.editor.Document;
import com.intellij.openapi.fileEditor.FileDocumentManager;
import com.intellij.openapi.fileEditor.FileDocumentManagerListener;
import com.intellij.openapi.project.Project;
import com.intellij.openapi.vfs.VirtualFile;
import org.getrafty.fragments.services.FragmentsIndex;
import org.getrafty.fragments.services.FragmentsManager;
import org.jetbrains.annotations.NotNull;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class FileSaveListener implements FileDocumentManagerListener {
    private static final Pattern SNIPPET_PATTERN = Pattern.compile(
            "// ==== YOUR CODE: @(.*?) ====(.*?)// ==== END YOUR CODE ====",
            Pattern.DOTALL
    );

    private final FragmentsManager snippetService;
    private final FragmentsIndex snippetIndexService;

    public FileSaveListener(@NotNull Project project) {
        this.snippetService = project.getService(FragmentsManager.class);
        this.snippetIndexService = project.getService(FragmentsIndex.class);
    }

    @Override
    public void beforeDocumentSaving(@NotNull Document document) {
        String text = document.getText();
        VirtualFile file = FileDocumentManager.getInstance().getFile(document);

        if (file == null) return;

        FragmentsIndexUtils.reindexFile(file, snippetIndexService);

        Matcher matcher = SNIPPET_PATTERN.matcher(text);

        while (matcher.find()) {
            String hash = matcher.group(1).trim();
            String snippetContent = matcher.group(2);
            snippetService.saveSnippet(hash, snippetContent);
        }
    }
}
