package org.getrafty.fragments.inspection;

import com.intellij.openapi.editor.Editor;
import com.intellij.openapi.editor.markup.EffectType;
import com.intellij.openapi.editor.markup.HighlighterTargetArea;
import com.intellij.openapi.editor.markup.MarkupModel;
import com.intellij.openapi.editor.markup.TextAttributes;
import com.intellij.openapi.vfs.VirtualFile;
import com.intellij.ui.JBColor;
import org.getrafty.fragments.services.FragmentsIndex;
import org.jetbrains.annotations.NotNull;

import java.awt.*;
import java.util.List;

public class FragmentCollisionHighlighter {
    private final FragmentsIndex snippetIndexService;

    public FragmentCollisionHighlighter(@NotNull FragmentsIndex snippetIndexService) {
        this.snippetIndexService = snippetIndexService;
    }

    public void highlightDuplicates(@NotNull Editor editor) {
        VirtualFile file = editor.getVirtualFile();
        if (file == null) return;

        MarkupModel markupModel = editor.getMarkupModel();

        snippetIndexService.getAllSnippets().forEach((hash, fileOccurrences) -> {
            List<FragmentsIndex.OffsetRange> offsets = snippetIndexService.getOffsetsForFile(hash, file);
            if (fileOccurrences.size() > 1 || offsets.size() > 1) { // More than one file contains this snippet
                offsets.forEach(offsetRange -> addHighlight(markupModel, offsetRange));
            }
        });
    }

    private void addHighlight(@NotNull MarkupModel markupModel, @NotNull FragmentsIndex.OffsetRange offsetRange) {
        TextAttributes attributes = new TextAttributes();
        attributes.setBackgroundColor(new JBColor(new Color(255, 200, 200), new Color(255, 200, 200)));
        attributes.setEffectColor(JBColor.RED);
        attributes.setEffectType(EffectType.BOXED);

        markupModel.addRangeHighlighter(
                offsetRange.start(),
                offsetRange.end(),
                0,
                attributes,
                HighlighterTargetArea.EXACT_RANGE
        );
    }

    public void clearHighlights(@NotNull Editor editor) {
        MarkupModel markupModel = editor.getMarkupModel();
        markupModel.removeAllHighlighters();
    }
}
