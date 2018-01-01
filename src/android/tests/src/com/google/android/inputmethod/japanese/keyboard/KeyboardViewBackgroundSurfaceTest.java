// Copyright 2010-2018, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

package org.mozc.android.inputmethod.japanese.keyboard;

import static org.easymock.EasyMock.expect;

import org.mozc.android.inputmethod.japanese.keyboard.BackgroundDrawableFactory.DrawableType;
import org.mozc.android.inputmethod.japanese.keyboard.Flick.Direction;
import org.mozc.android.inputmethod.japanese.keyboard.Key.Stick;
import org.mozc.android.inputmethod.japanese.keyboard.KeyState.MetaState;
import org.mozc.android.inputmethod.japanese.keyboard.Keyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.keyboard.KeyboardViewBackgroundSurface.SurfaceCanvas;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.testing.MockResourcesWithDisplayMetrics;
import org.mozc.android.inputmethod.japanese.view.DrawableCache;
import com.google.common.base.Optional;

import android.content.res.Resources;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.test.MoreAsserts;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.easymock.IMocksControl;

import java.util.Arrays;
import java.util.Collections;
import java.util.EnumSet;

/**
 */
public class KeyboardViewBackgroundSurfaceTest extends InstrumentationTestCaseWithMock {

  private static final int WIDTH = 50;
  private static final int HEIGHT = 30;
  private static final int HORIZONTAL_GAP = 4;

  private DrawableCache createDrawableCacheMock() {
    return createMockBuilder(DrawableCache.class)
        .withConstructor(Resources.class)
        .withArgs(new MockResourcesWithDisplayMetrics())
        .createMock();
  }

  private static Key createDummyKey(int x, int y, int iconResourceId, DrawableType drawableType) {
    KeyEntity keyEntity = new KeyEntity(
        1, 'a', KeyEntity.INVALID_KEY_CODE, true, iconResourceId, Optional.<String>absent(),
        false, Optional.<PopUp>absent(), 0, 0, Integer.MAX_VALUE, Integer.MAX_VALUE);
    Flick centerFlick = new Flick(Direction.CENTER, keyEntity);
    KeyState keyState = new KeyState("",
                                     Collections.<MetaState>emptySet(),
                                     Collections.<MetaState>emptySet(),
                                     Collections.<MetaState>emptySet(),
                                     Collections.singletonList(centerFlick));
    return new Key(x, y, WIDTH, HEIGHT, HORIZONTAL_GAP, 0, false, false,
                   Stick.EVEN, drawableType, Collections.singletonList(keyState));
  }

  private static KeyEntity createInvalidKeyEntity(
      int sourceId, int keyCode, int keyIconResourceId) {
    return new KeyEntity(
        sourceId, keyCode, KeyEntity.INVALID_KEY_CODE, true, keyIconResourceId,
        Optional.<String>absent(), false, Optional.<PopUp>absent(),
        0, 0, Integer.MAX_VALUE, Integer.MAX_VALUE);
  }

  private static KeyEntity createInvalidKeyEntity() {
    return createInvalidKeyEntity(1, 'a', 0);
  }

  @SmallTest
  public void testGetKeyEntityForRendering() {
    KeyEntity centerEntity = createInvalidKeyEntity(1, 'a', 0);
    KeyEntity leftEntity = createInvalidKeyEntity(2, 'b', 0);
    Flick centerFlick = new Flick(Direction.CENTER, centerEntity);
    Flick leftFlick = new Flick(Direction.LEFT, leftEntity);

    Key key = new Key(
        0, 0, WIDTH, HEIGHT, HORIZONTAL_GAP, 0, false, false, Stick.EVEN,
        DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND,
        Collections.singletonList(
            new KeyState("",
                         Collections.<MetaState>emptySet(),
                         Collections.<MetaState>emptySet(),
                         Collections.<MetaState>emptySet(),
                         Arrays.asList(centerFlick, leftFlick))));

    // If this is the released key, the default key entity (center's one) should be returned.
    assertSame(
        centerEntity,
        KeyboardViewBackgroundSurface.getKeyEntityForRendering(
            key, Collections.<MetaState>emptySet(), Optional.<Direction>absent()).get());

    // If this is the pressed key without flick, the center key entity should be returned.
    assertSame(
        centerEntity,
        KeyboardViewBackgroundSurface.getKeyEntityForRendering(
            key, Collections.<MetaState>emptySet(), Optional.of(Direction.CENTER)).get());

    // If the flick state is set to the pressedKey, it should be returned.
    assertSame(
        leftEntity,
        KeyboardViewBackgroundSurface.getKeyEntityForRendering(
            key, Collections.<MetaState>emptySet(), Optional.of(Direction.LEFT)).get());

    // If the key doesn't have a KeyEntity for the current state, the default key entity
    // should be returned.
    assertSame(
        centerEntity,
        KeyboardViewBackgroundSurface.getKeyEntityForRendering(
            key, EnumSet.of(MetaState.CAPS_LOCK), Optional.of(Direction.RIGHT)).get());
    assertSame(
        centerEntity,
        KeyboardViewBackgroundSurface.getKeyEntityForRendering(
            key, EnumSet.of(MetaState.CAPS_LOCK), Optional.<Direction>absent()).get());
    assertSame(
        centerEntity,
        KeyboardViewBackgroundSurface.getKeyEntityForRendering(
            key, EnumSet.of(MetaState.CAPS_LOCK), Optional.of(Direction.CENTER)).get());
  }

  @SmallTest
  public void testGetKeyBackground_withBackground() {
    BackgroundDrawableFactory factory =
        new BackgroundDrawableFactory(createNiceMock(MockResourcesWithDisplayMetrics.class));

    Drawable background = factory.getDrawable(DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND);
    replayAll();

    Key key = createDummyKey(0, 0, 0, DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND);
    KeyboardViewBackgroundSurface surface = new KeyboardViewBackgroundSurface(
        factory, new DrawableCache(new MockResourcesWithDisplayMetrics()));

    // The icon should be returned with appropriate state.
    assertSame(background, surface.getKeyBackground(key, false).get());
    MoreAsserts.assertEquals(new int[]{}, background.getState());

    assertSame(background, surface.getKeyBackground(key, true).get());
    MoreAsserts.assertEquals(new int[]{ android.R.attr.state_pressed }, background.getState());
  }

  @SmallTest
  public void testGetKeyIcon_withoutIcon() {
    DrawableCache drawableCache = new DrawableCache(new MockResourcesWithDisplayMetrics());

    // Optional.absent() should be returned for Optional.absent() KeyEntity.
    assertFalse(KeyboardViewBackgroundSurface.getKeyIcon(
        drawableCache, Optional.<KeyEntity>absent(), true).isPresent());
    assertFalse(KeyboardViewBackgroundSurface.getKeyIcon(
        drawableCache, Optional.<KeyEntity>absent(), false).isPresent());

    // Optional.absent() should be returned for a KeyEntity without icon.
    KeyEntity keyEntity = createInvalidKeyEntity();
    assertFalse(KeyboardViewBackgroundSurface.getKeyIcon(
        drawableCache, Optional.of(keyEntity), true).isPresent());
    assertFalse(KeyboardViewBackgroundSurface.getKeyIcon(
        drawableCache, Optional.of(keyEntity), false).isPresent());
  }

  @SmallTest
  public void testGetKeyIcon_withIcon() {
    Drawable icon = new ColorDrawable();
    int iconResourceId = 1;
    DrawableCache drawableCache = createDrawableCacheMock();
    expect(drawableCache.getDrawable(iconResourceId)).andStubReturn(Optional.of(icon));
    replayAll();
    KeyEntity keyEntity = createInvalidKeyEntity(1, 'a', iconResourceId);

    // The icon should be returned with appropriate state.
    assertSame(icon, KeyboardViewBackgroundSurface.getKeyIcon(
        drawableCache, Optional.of(keyEntity), false).get());
    MoreAsserts.assertEquals(new int[]{}, icon.getState());

    assertSame(icon, KeyboardViewBackgroundSurface.getKeyIcon(
        drawableCache, Optional.of(keyEntity), true).get());
    MoreAsserts.assertEquals(new int[]{ android.R.attr.state_pressed }, icon.getState());
  }

  @MediumTest
  public void testSenarioTest() {
    IMocksControl drawableCacheControl = createControl();
    DrawableCache drawableCache = createMockBuilder(DrawableCache.class)
        .withConstructor(Resources.class)
        .withArgs(new MockResourcesWithDisplayMetrics())
        .createMock(drawableCacheControl);

    Drawable icon1 = new ColorDrawable();
    int icon1ResourceId = 1;
    Key key1 =
        createDummyKey(0, 0, icon1ResourceId, DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND);
    expect(drawableCache.getDrawable(icon1ResourceId)).andStubReturn(Optional.of(icon1));

    Drawable icon2 = new ColorDrawable();
    int icon2ResourceId = 3;
    Key key2 =
        createDummyKey(WIDTH, 0, icon2ResourceId, DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND);
    expect(drawableCache.getDrawable(icon2ResourceId)).andStubReturn(Optional.of(icon2));

    Drawable icon3 = new ColorDrawable();
    int icon3ResourceId = 5;
    Key key3 = createDummyKey(
        WIDTH * 2, 0, icon3ResourceId, DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND);
    expect(drawableCache.getDrawable(icon3ResourceId)).andStubReturn(Optional.of(icon3));

    Drawable icon4 = new ColorDrawable();
    int icon4ResourceId = 7;
    Key key4 = createDummyKey(
        0, HEIGHT, icon4ResourceId, DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND);
    expect(drawableCache.getDrawable(icon4ResourceId)).andStubReturn(Optional.of(icon4));

    Drawable icon5 = new ColorDrawable();
    int icon5ResourceId = 9;
    Key key5 = createDummyKey(
        WIDTH, HEIGHT, icon5ResourceId, DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND);
    expect(drawableCache.getDrawable(icon5ResourceId)).andStubReturn(Optional.of(icon5));

    Drawable icon6 = new ColorDrawable();
    int icon6ResourceId = 11;
    Key key6 = createDummyKey(
        WIDTH * 2, HEIGHT, icon6ResourceId, DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND);
    expect(drawableCache.getDrawable(icon6ResourceId)).andStubReturn(Optional.of(icon6));

    Drawable icon7 = new ColorDrawable();
    Drawable leftIcon7 = new ColorDrawable();
    int icon7ResourceId = 13;
    int leftIcon7ResourceId = 15;
    Flick centerFlick7 =
        new Flick(Direction.CENTER, createInvalidKeyEntity(1, 'a', icon7ResourceId));
    Flick leftFlick7 =
        new Flick(Direction.LEFT, createInvalidKeyEntity(2, 'a', leftIcon7ResourceId));
    KeyState keyState = new KeyState("",
                                     Collections.<MetaState>emptySet(),
                                     Collections.<MetaState>emptySet(),
                                     Collections.<MetaState>emptySet(),
                                     Arrays.asList(centerFlick7, leftFlick7));
    Key key7 = new Key(0, HEIGHT * 2, WIDTH, HEIGHT, HORIZONTAL_GAP, 0, false, false,
                       Stick.EVEN, DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND,
                       Collections.singletonList(keyState));
    expect(drawableCache.getDrawable(icon7ResourceId)).andStubReturn(Optional.of(icon7));
    expect(drawableCache.getDrawable(leftIcon7ResourceId)).andStubReturn(Optional.of(leftIcon7));

    Drawable icon8 = new ColorDrawable();
    int icon8ResourceId = 17;
    Key key8 = createDummyKey(
        WIDTH, HEIGHT * 2, icon8ResourceId, DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND);
    expect(drawableCache.getDrawable(icon8ResourceId)).andStubReturn(Optional.of(icon8));

    Drawable icon9 = new ColorDrawable();
    int icon9ResourceId = 19;
    Key key9 = createDummyKey(
        WIDTH * 2, HEIGHT * 2, icon9ResourceId, DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND);
    expect(drawableCache.getDrawable(icon9ResourceId)).andStubReturn(Optional.of(icon9));
    drawableCacheControl.replay();

    Keyboard keyboard = new Keyboard(
        Optional.<String>absent(),
        Arrays.asList(
            new Row(Arrays.asList(key1, key2, key3), HEIGHT, 0),
            new Row(Arrays.asList(key4, key5, key6), HEIGHT, 0),
            new Row(Arrays.asList(key7, key8, key9), HEIGHT, 0)),
        1,
        KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_KANA);

    IMocksControl canvasControl = createControl();
    final SurfaceCanvas canvas = canvasControl.createMock(SurfaceCanvas.class);
    int expectedKeyWidth = WIDTH - HORIZONTAL_GAP;
    int expectedKeyHeight = HEIGHT;

    BackgroundDrawableFactory factory =
        new BackgroundDrawableFactory(createNiceMock(MockResourcesWithDisplayMetrics.class));
    Drawable background = factory.getDrawable(DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND);

    // The first case is just a simple senario, just setting a new keyboard.
    canvas.drawDrawable(background, 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon1, 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH + 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon2, WIDTH + 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH * 2 + 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon3, WIDTH * 2 + 2, 0, expectedKeyWidth, expectedKeyHeight);

    canvas.drawDrawable(background, 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon4, 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH + 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon5, WIDTH + 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH * 2 + 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon6, WIDTH * 2 + 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);

    canvas.drawDrawable(background, 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon7, 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH + 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon8, WIDTH + 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(
        background, WIDTH * 2 + 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon9, WIDTH * 2 + 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvasControl.replay();

    KeyboardViewBackgroundSurface surface =
        new KeyboardViewBackgroundSurface(factory, drawableCache);

    surface.reset(Optional.of(keyboard), Collections.<MetaState>emptySet());
    surface.draw(canvas);

    canvasControl.verify();

    // The second case is just updating some keys.
    surface.clearPressedKey();
    canvasControl.reset();
    canvas.drawDrawable(background, 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon1, 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH + 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon2, WIDTH + 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH * 2 + 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon3, WIDTH * 2 + 2, 0, expectedKeyWidth, expectedKeyHeight);

    canvas.drawDrawable(background, 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon4, 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH + 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon5, WIDTH + 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH * 2 + 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon6, WIDTH * 2 + 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);

    canvas.drawDrawable(background, 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        leftIcon7, 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH + 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon8, WIDTH + 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(
        background, WIDTH * 2 + 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon9, WIDTH * 2 + 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);

    canvasControl.replay();

    surface.addPressedKey(key5, Direction.CENTER);
    surface.addPressedKey(key7, Direction.LEFT);
    surface.draw(canvas);

    canvasControl.verify();

    // The third senario is updating some keys multiple times.
    surface.clearPressedKey();
    canvasControl.reset();
    canvas.drawDrawable(background, 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon1, 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH + 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon2, WIDTH + 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH * 2 + 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon3, WIDTH * 2 + 2, 0, expectedKeyWidth, expectedKeyHeight);

    canvas.drawDrawable(background, 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon4, 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH + 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon5, WIDTH + 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH * 2 + 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon6, WIDTH * 2 + 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);

    canvas.drawDrawable(background, 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon7, 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH + 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon8, WIDTH + 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(
        background, WIDTH * 2 + 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon9, WIDTH * 2 + 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);

    canvasControl.replay();

    surface.addPressedKey(key8, Direction.CENTER);
    surface.addPressedKey(key8, Direction.LEFT);
    surface.addPressedKey(key8, Direction.RIGHT);
    surface.addPressedKey(key8, Direction.CENTER);
    surface.addPressedKey(key8, Direction.UP);
    surface.addPressedKey(key8, Direction.DOWN);
    surface.removePressedKey(key8);

    surface.addPressedKey(key1, Direction.CENTER);
    surface.addPressedKey(key1, Direction.UP);
    surface.addPressedKey(key1, Direction.DOWN);
    surface.addPressedKey(key1, Direction.CENTER);

    surface.draw(canvas);

    canvasControl.verify();

    // The fourth senario is updating some keys, and then setting the keyboard.
    surface.clearPressedKey();
    canvasControl.reset();
    canvas.drawDrawable(background, 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon1, 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH + 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon2, WIDTH + 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH * 2 + 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon3, WIDTH * 2 + 2, 0, expectedKeyWidth, expectedKeyHeight);

    canvas.drawDrawable(background, 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon4, 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH + 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon5, WIDTH + 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH * 2 + 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon6, WIDTH * 2 + 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);

    canvas.drawDrawable(background, 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon7, 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH + 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon8, WIDTH + 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(
        background, WIDTH * 2 + 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon9, WIDTH * 2 + 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvasControl.replay();

    surface.addPressedKey(key5, Direction.CENTER);
    surface.addPressedKey(key7, Direction.LEFT);

    surface.reset(Optional.of(keyboard), Collections.<MetaState>emptySet());
    surface.draw(canvas);

    canvasControl.verify();

    // The fifth senario is updating some keys, setting the keyboard, and then updating some keys.
    surface.clearPressedKey();
    canvasControl.reset();
    canvas.drawDrawable(background, 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon1, 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH + 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon2, WIDTH + 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH * 2 + 2, 0, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon3, WIDTH * 2 + 2, 0, expectedKeyWidth, expectedKeyHeight);

    canvas.drawDrawable(background, 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon4, 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH + 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon5, WIDTH + 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH * 2 + 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon6, WIDTH * 2 + 2, HEIGHT, expectedKeyWidth, expectedKeyHeight);

    canvas.drawDrawable(background, 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        leftIcon7, 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(background, WIDTH + 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon8, WIDTH + 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawable(
        background, WIDTH * 2 + 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        icon9, WIDTH * 2 + 2, HEIGHT * 2, expectedKeyWidth, expectedKeyHeight);
    canvasControl.replay();

    surface.addPressedKey(key5, Direction.CENTER);
    surface.addPressedKey(key2, Direction.LEFT);

    surface.reset(Optional.of(keyboard), Collections.<MetaState>emptySet());

    surface.addPressedKey(key8, Direction.CENTER);
    surface.addPressedKey(key8, Direction.LEFT);
    surface.addPressedKey(key8, Direction.RIGHT);
    surface.addPressedKey(key8, Direction.CENTER);
    surface.addPressedKey(key8, Direction.UP);
    surface.addPressedKey(key8, Direction.DOWN);
    surface.removePressedKey(key8);

    surface.addPressedKey(key1, Direction.CENTER);
    surface.addPressedKey(key1, Direction.UP);
    surface.addPressedKey(key1, Direction.DOWN);
    surface.addPressedKey(key1, Direction.CENTER);

    surface.addPressedKey(key7, Direction.LEFT);

    surface.draw(canvas);

    canvasControl.verify();
  }
}
