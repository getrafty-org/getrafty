package org.getrafty.fragments.listeners;

import com.intellij.openapi.editor.Editor;
import com.intellij.openapi.fileEditor.FileEditorManager;
import com.intellij.openapi.fileEditor.FileEditorManagerListener;
import com.intellij.openapi.project.Project;
import com.intellij.openapi.vfs.VirtualFile;
import org.getrafty.fragments.inspection.FragmentCollisionHighlighter;
import org.getrafty.fragments.services.FragmentsIndex;
import org.jetbrains.annotations.NotNull;

public class FileOpenListener implements FileEditorManagerListener {
    private final FragmentsIndex snippetIndexService;
    private final FragmentCollisionHighlighter duplicateSnippetHighlighter;

    public FileOpenListener(@NotNull Project project) {
        this.snippetIndexService = project.getService(FragmentsIndex.class);
        this.duplicateSnippetHighlighter = new FragmentCollisionHighlighter(snippetIndexService);
    }

    @Override
    public void fileOpened(@NotNull FileEditorManager source, @NotNull VirtualFile file) {
        Editor editor = source.getSelectedTextEditor();

        if (editor != null) {
            // Use SnippetIndexer to reindex the file
            FragmentsIndexUtils.reindexFile(file, snippetIndexService);

            // Highlight duplicates in the editor
            duplicateSnippetHighlighter.highlightDuplicates(editor);
        }
    }
}
