package org.getrafty.snippy.actions;

import com.intellij.openapi.actionSystem.AnAction;
import com.intellij.openapi.actionSystem.AnActionEvent;
import com.intellij.openapi.fileEditor.FileEditor;
import com.intellij.openapi.fileEditor.FileEditorManager;
import com.intellij.openapi.fileEditor.TextEditor;
import com.intellij.openapi.project.Project;
import com.intellij.openapi.wm.StatusBar;
import com.intellij.openapi.wm.WindowManager;
import org.getrafty.snippy.services.SnippetService;
import org.getrafty.snippy.status.ModeStatusWidget;

public class ToggleSnippetFocusAction extends AnAction {

    @Override
    public void actionPerformed(AnActionEvent e) {
        Project project = e.getProject();
        if (project == null) {
            return; // No project found; safely exit
        }

        // Fetch the SnippetService dynamically
        SnippetService snippetService = project.getService(SnippetService.class);

        // Toggle the mode
        snippetService.toggleMode();

        // Update all open text editors
        FileEditorManager fileEditorManager = FileEditorManager.getInstance(project);
        for (FileEditor fileEditor : fileEditorManager.getAllEditors()) {
            if (fileEditor instanceof TextEditor) {
                var editor = ((TextEditor) fileEditor).getEditor();
                snippetService.reloadSnippets(editor.getDocument());
            }
        }

        // Update the status bar widget
        StatusBar statusBar = WindowManager.getInstance().getStatusBar(project);
        if (statusBar != null) {
            ModeStatusWidget widget = (ModeStatusWidget) statusBar.getWidget("SnippetModeWidget");
            if (widget != null) {
                statusBar.updateWidget(widget.ID());
            }
        }
    }
}
