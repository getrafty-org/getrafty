package org.getrafty.fragments.services;

import com.intellij.openapi.components.Service;
import com.intellij.openapi.editor.Document;
import com.intellij.openapi.command.WriteCommandAction;
import org.getrafty.fragments.dao.FragmentsDao;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

@Service(Service.Level.PROJECT)
public final class FragmentsManager {
    private final FragmentsDao fragmentsDao;
    private boolean isMaintainerMode = false; // Default mode

    public FragmentsManager(com.intellij.openapi.project.Project project) {
        this.fragmentsDao = new FragmentsDao(project);
    }

    public void saveSnippet(String hash, String content) {
        String mode = isMaintainerMode ? "maintainer" : "user";
        fragmentsDao.saveFragment(hash, content, mode);
    }


    public void reloadSnippets(Document document) {
        String text = document.getText();

        Pattern pattern = Pattern.compile("// ==== YOUR CODE: @(.*?) ====(.*?)// ==== END YOUR CODE ====", Pattern.DOTALL);
        Matcher matcher = pattern.matcher(text);

        StringBuilder updatedText = new StringBuilder();
        int lastMatchEnd = 0;

        while (matcher.find()) {
            String snippetId = matcher.group(1);
            String currentContent = matcher.group(2);
            String newContent = loadSnippet(snippetId);

            updatedText.append(text, lastMatchEnd, matcher.start());
            updatedText.append("// ==== YOUR CODE: @").append(snippetId).append(" ====");
            updatedText.append(newContent != null ? newContent : currentContent);
            updatedText.append("// ==== END YOUR CODE ====");
            lastMatchEnd = matcher.end();
        }

        updatedText.append(text.substring(lastMatchEnd));

        WriteCommandAction.runWriteCommandAction(null, () -> {
            document.setText(updatedText.toString());
        });
    }

    public String loadSnippet(String snippetId) {
        String mode = isMaintainerMode ? "maintainer" : "user";
        return fragmentsDao.findFragment(snippetId, mode);
    }

    public void toggleMode() {
        isMaintainerMode = !isMaintainerMode;
    }

    public boolean isMaintainerMode() {
        return isMaintainerMode;
    }
}
