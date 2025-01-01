package org.getrafty.fragments.listeners;

import com.intellij.openapi.components.Service;
import com.intellij.openapi.editor.Editor;
import com.intellij.openapi.fileEditor.FileDocumentManager;
import com.intellij.openapi.fileEditor.FileEditorManager;
import com.intellij.openapi.project.Project;
import com.intellij.openapi.vfs.VirtualFile;
import com.intellij.openapi.vfs.VirtualFileEvent;
import com.intellij.openapi.vfs.VirtualFileListener;
import org.getrafty.fragments.FragmentUtils;
import org.getrafty.fragments.inspection.FragmentCollisionHighlighter;
import org.getrafty.fragments.services.FragmentsIndex;
import org.jetbrains.annotations.NotNull;

@Service(Service.Level.PROJECT)
public final class FileChangeListener implements VirtualFileListener {
    private final Project project;
    private final FragmentsIndex snippetIndexService;
    private final FragmentCollisionHighlighter duplicateSnippetHighlighter;

    public FileChangeListener(@NotNull Project project) {
        this.project = project;
        this.snippetIndexService = project.getService(FragmentsIndex.class);
        this.duplicateSnippetHighlighter = project.getService(FragmentCollisionHighlighter.class);
    }

    @Override
    public void contentsChanged(@NotNull VirtualFileEvent event) {
        VirtualFile file = event.getFile();
        if (file.isDirectory()) return;

        reindexFileAndUpdateHighlights(file);
    }

    @Override
    public void fileCreated(@NotNull VirtualFileEvent event) {
        VirtualFile file = event.getFile();
        if (file.isDirectory()) return;

        reindexFileAndUpdateHighlights(file);
    }

    @Override
    public void fileDeleted(@NotNull VirtualFileEvent event) {
        VirtualFile file = event.getFile();
        if (file.isDirectory()) return;

        snippetIndexService.forgetFragmentEntriesForFile(file);
        clearHighlights(file); // Clear highlights if the file is deleted
    }

    private void reindexFileAndUpdateHighlights(@NotNull VirtualFile file) {
        // Reindex the file
        var document = FileDocumentManager.getInstance().getDocument(file);
        if (document != null) {
            snippetIndexService.forgetFragmentEntriesForFile(file); // Clear the old index for the file
            FragmentUtils.reindexFile(file, snippetIndexService); // Reindex the file

            // Clear old highlights and apply updated ones
            clearHighlights(file);
            highlightFileDuplicates(file);
        }
    }

    private void highlightFileDuplicates(@NotNull VirtualFile file) {
        var editorManager = FileEditorManager.getInstance(project);
        Editor editor = editorManager.getSelectedTextEditor();

        if (editor != null && editor.getDocument() == FileDocumentManager.getInstance().getDocument(file)) {
            duplicateSnippetHighlighter.highlightDuplicates(editor); // Apply duplicate highlights
        }
    }

    private void clearHighlights(@NotNull VirtualFile file) {
        var editorManager = FileEditorManager.getInstance(project);
        Editor editor = editorManager.getSelectedTextEditor();

        if (editor != null && editor.getDocument() == FileDocumentManager.getInstance().getDocument(file)) {
            duplicateSnippetHighlighter.clearHighlights(editor); // Clear all highlights for this file
        }
    }
}
