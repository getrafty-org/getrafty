package org.getrafty.fragments.actions;

import com.intellij.openapi.actionSystem.AnAction;
import com.intellij.openapi.actionSystem.AnActionEvent;
import com.intellij.openapi.command.WriteCommandAction;
import com.intellij.openapi.editor.Caret;
import com.intellij.openapi.editor.Document;
import com.intellij.openapi.editor.Editor;
import com.intellij.openapi.project.Project;
import com.intellij.openapi.ui.Messages;
import org.jetbrains.annotations.NotNull;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class InsertFragmentAction extends AnAction {

    private static final Pattern SNIPPET_PATTERN = Pattern.compile("// ==== YOUR CODE: @(.*?) ====(.*?)// ==== END YOUR CODE ====", Pattern.DOTALL);

    @Override
    public void actionPerformed(@NotNull AnActionEvent event) {
        Editor editor = event.getData(com.intellij.openapi.actionSystem.CommonDataKeys.EDITOR);
        Project project = event.getProject();

        if (editor == null || project == null) {
            Messages.showErrorDialog("No active editor or project found!", "Error");
            return;
        }

        Document document = editor.getDocument();
        Caret caret = editor.getCaretModel().getCurrentCaret();
        int caretOffset = caret.getOffset();

        // Check if the caret is inside an existing snippet
        if (isCaretInsideSnippet(document, caretOffset)) {
            Messages.showErrorDialog("Fragments can't overlap", "Error");
            return;
        }

        // Generate a unique hash for the marker
        String hash = generateHash();

        // Create the marker text
        String startMarker = "// ==== YOUR CODE: @" + hash + " ====\n";
        String endMarker = "// ==== END YOUR CODE ====\n";

        // Insert the markers into the document
        WriteCommandAction.runWriteCommandAction(project, () -> {
            document.insertString(caretOffset, startMarker + "\n" + endMarker);
        });
    }

    private boolean isCaretInsideSnippet(@NotNull Document document, int caretOffset) {
        String text = document.getText();
        Matcher matcher = SNIPPET_PATTERN.matcher(text);

        while (matcher.find()) {
            int snippetStart = matcher.start(2); // Start of snippet content
            int snippetEnd = matcher.end(2); // End of snippet content

            if (caretOffset >= snippetStart && caretOffset <= snippetEnd) {
                return true; // Caret is inside an existing snippet
            }
        }
        return false;
    }

    private String generateHash() {
        return java.util.UUID.randomUUID().toString().substring(0, 8); // Simplified unique ID
    }
}
