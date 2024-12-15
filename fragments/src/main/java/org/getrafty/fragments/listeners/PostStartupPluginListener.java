package org.getrafty.fragments.listeners;

import com.intellij.openapi.fileEditor.FileDocumentManagerListener;
import com.intellij.openapi.fileEditor.FileEditorManagerListener;
import com.intellij.openapi.project.Project;
import com.intellij.openapi.startup.ProjectActivity;
import com.intellij.openapi.vfs.VirtualFileManager;
import kotlin.Unit;
import kotlin.coroutines.Continuation;
import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;

public class PostStartupPluginListener implements ProjectActivity {

    @Override
    public @Nullable Object execute(@NotNull Project project, @NotNull Continuation<? super Unit> continuation) {
        project.getMessageBus().connect().subscribe(
                FileDocumentManagerListener.TOPIC,
                new FileSaveListener(project)
        );

        project.getMessageBus()
                .connect()
                .subscribe(FileEditorManagerListener.FILE_EDITOR_MANAGER, new FileOpenListener(project));

        // Register SnippetFileChangeListener for file creation, deletion, and modification
        VirtualFileManager.getInstance().addVirtualFileListener(
                new FileChangeListener(project),
                project
        );

        return Unit.INSTANCE;
    }
}
