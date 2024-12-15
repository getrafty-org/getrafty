package org.getrafty.fragments.actions;

import com.intellij.openapi.actionSystem.AnAction;
import com.intellij.openapi.actionSystem.AnActionEvent;
import com.intellij.openapi.fileEditor.FileEditor;
import com.intellij.openapi.fileEditor.FileEditorManager;
import com.intellij.openapi.fileEditor.TextEditor;
import com.intellij.openapi.wm.WindowManager;
import org.getrafty.fragments.services.FragmentsManager;
import org.getrafty.fragments.status.FragmentViewWidget;

public class ToggleFragmentsViewAction extends AnAction {
    @Override
    public void actionPerformed(AnActionEvent e) {
        var project = e.getProject();
        if (project == null) {
            return;
        }

        var fragmentsManager = project.getService(FragmentsManager.class);
        fragmentsManager.toggleMode();

        var fileEditorManager = FileEditorManager.getInstance(project);
        for (FileEditor fileEditor : fileEditorManager.getAllEditors()) {
            if (fileEditor instanceof TextEditor) {
                var editor = ((TextEditor) fileEditor).getEditor();
                fragmentsManager.reloadSnippets(editor.getDocument());
            }
        }

        var statusBar = WindowManager.getInstance().getStatusBar(project);
        if (statusBar != null) {
            var widget = (FragmentViewWidget) statusBar.getWidget(FragmentViewWidget.FRAGMENT_VIEW_WIDGET);
            if (widget != null) {
                statusBar.updateWidget(widget.ID());
            }
        }
    }
}
