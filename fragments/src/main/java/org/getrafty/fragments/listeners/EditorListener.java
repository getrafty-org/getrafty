package org.getrafty.fragments.listeners;

import com.intellij.openapi.diagnostic.Logger;
import com.intellij.openapi.editor.Editor;
import com.intellij.openapi.editor.event.CaretListener;
import com.intellij.openapi.editor.event.EditorFactoryEvent;
import com.intellij.openapi.editor.event.EditorFactoryListener;
import com.intellij.openapi.project.Project;
import com.intellij.openapi.util.Disposer;
import org.jetbrains.annotations.NotNull;

/**
 * Listens for editor creation events and attaches a {@link FragmentCaretListener}.
 */
public class EditorListener implements EditorFactoryListener {

    private static final Logger LOG = Logger.getInstance(EditorListener.class);

    private final Project project;

    public EditorListener(Project project) {
        this.project = project;
    }

    @Override
    public void editorCreated(@NotNull EditorFactoryEvent event) {
        Editor editor = event.getEditor();

        if (editor.getProject() != project) {
            return; // Ignore editors not associated with this project
        }

        LOG.info("Editor created for project: " + project.getName());

        // Attach the FragmentCaretListener
        FragmentCaretListener fragmentCaretListener = new FragmentCaretListener(editor);
        editor.getCaretModel().addCaretListener(fragmentCaretListener);

        // Ensure the listener is removed when the editor is released
        Disposer.register(project, () -> {
            editor.getCaretModel().removeCaretListener(fragmentCaretListener);
            LOG.info("CaretListener removed from editor: " + editor);
        });

        LOG.info("CaretListener attached to editor: " + editor);
    }

    @Override
    public void editorReleased(@NotNull EditorFactoryEvent event) {
        LOG.info("Editor released: " + event.getEditor());
    }
}
