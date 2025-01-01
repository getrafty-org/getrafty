package org.getrafty.fragments.listeners;

import com.intellij.openapi.editor.Document;
import com.intellij.openapi.fileEditor.FileDocumentManager;
import com.intellij.openapi.fileEditor.FileDocumentManagerListener;
import com.intellij.openapi.project.Project;
import com.intellij.openapi.vfs.VirtualFile;
import org.getrafty.fragments.FragmentUtils;
import org.getrafty.fragments.services.FragmentsIndex;
import org.getrafty.fragments.services.FragmentsManager;
import org.jetbrains.annotations.NotNull;

import java.util.regex.Matcher;

import static org.getrafty.fragments.FragmentUtils.FRAGMENT_PATTERN;

public class FileSaveListener implements FileDocumentManagerListener {
    private final FragmentsManager fragmentsManager;
    private final FragmentsIndex fragmentsIndex;

    public FileSaveListener(@NotNull Project project) {
        this.fragmentsManager = project.getService(FragmentsManager.class);
        this.fragmentsIndex = project.getService(FragmentsIndex.class);
    }

    @Override
    public void beforeDocumentSaving(@NotNull Document document) {
        String text = document.getText();
        VirtualFile file = FileDocumentManager.getInstance().getFile(document);

        if (file == null) return;

        FragmentUtils.reindexFile(file, fragmentsIndex);

        // TODO: Extract to fragment parser
        Matcher matcher = FRAGMENT_PATTERN.matcher(text);

        while (matcher.find()) {
            var fragmentId = matcher.group(1).trim();
            var fragmentCode = matcher.group(2);
            fragmentsManager.saveFragment(fragmentId, fragmentCode);
        }
    }
}
