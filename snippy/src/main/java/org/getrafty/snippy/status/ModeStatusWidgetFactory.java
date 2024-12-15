package org.getrafty.snippy.status;

import com.intellij.openapi.project.Project;
import com.intellij.openapi.wm.StatusBarWidget;
import com.intellij.openapi.wm.StatusBarWidgetFactory;
import org.jetbrains.annotations.NotNull;

public class ModeStatusWidgetFactory implements StatusBarWidgetFactory {

    @NotNull
    @Override
    public String getId() {
        return "SnippetModeWidget";
    }

    @NotNull
    @Override
    public String getDisplayName() {
        return "Snippet Mode";
    }

    @Override
    public boolean isAvailable(@NotNull Project project) {
        return true;
    }

    @NotNull
    @Override
    public StatusBarWidget createWidget(@NotNull Project project) {
        return new ModeStatusWidget(project); // Returns a valid StatusBarWidget
    }

    @Override
    public boolean isConfigurable() {
        return false;
    }
}
